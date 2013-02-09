#define _GNU_SOURCE

#ifdef linux
#include <features.h>
#define _FILE_OFFSET_BITS
#endif

#include "message.h"

#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef linux
#include <sys/syscall.h>
#elif defined(__APPLE__)
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#endif

#define HAVE_JSON
#define HAVE_SYSLOG

#define USE_RWLOCK

#ifdef HAVE_JSON
#define __STRICT_ANSI__
#include <json/json.h>
#undef __STRICT_ANSI__
#endif  /* HAVE_JSON */

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

#define MCF_UTC 0x0001


struct msg_category {
  struct msg_category *next;
  struct msg_category *prev;
  char *name;

  int type;
  int level;
  unsigned index;               /* index of the msg_cmap::map array */
  int refcount;

  char *fmt_prologue;
  char *fmt_time;

  pthread_rwlock_t lock;

  ssize_t (*write)(struct msg_category *self, int level,
                   const void *data, size_t size);
  void (*close)(struct msg_category *self);
  void (*refresh)(struct msg_category *self);
};

struct msg_cmap {
  size_t size;
  struct msg_category **map;
#ifdef USE_RWLOCK
  pthread_rwlock_t *locks;
#else
  pthread_mutex_t *locks;
#endif

  pthread_t sigthread;
  void (*alt_huphandler)(int, siginfo_t *, void *);
  int pipefd[2];
};

#define MSG_CATEGORY_CMAP_SIZE  509

#ifdef USE_RWLOCK
#define RD_LOCK(x)      pthread_rwlock_rdlock(x)
#define WR_LOCK(x)      pthread_rwlock_wrlock(x)
#define UNLOCK(x)       pthread_rwlock_unlock(x)
#define INIT_LOCK(x)    pthread_rwlock_init(x, NULL)
#define DEL_LOCK(x)     pthread_rwlock_destroy(x)
#elif defined(USE_MUTEX)
#define RD_LOCK(x)      pthread_mutex_lock(x)
#define WR_LOCK(x)      pthread_mutex_lock(x)
#define UNLOCK(x)       pthread_mutex_unlock(x)
#define INIT_LOCK(x)    pthread_mutex_init(x, NULL)
#define DEL_LOCK(x)     pthread_mutex_destroy(x)
#else
#define RD_LOCK(x)      0
#define WR_LOCK(x)      0
#define UNLOCK(x)       0
#define INIT_LOCK(x)    0
#define DEL_LOCK(x)     0
#endif  /* USE_RWLOCK */

#define FILE_MODE       (O_CREAT | O_APPEND | O_CLOEXEC | O_WRONLY)
#define FILE_ACC        (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
struct category_file {
  struct msg_category base;

  int fd;
  char *pathname;
};

struct category_syslog {
  struct msg_category base;

  int option;
  int facility;
};

#define MC_MULTIPLEX_MAX        8

struct category_multiplex {
  struct msg_category base;

  size_t size;
  size_t capacity;
  struct msg_category **sink;
};

static struct msg_category *message_get_category_(const char *name,
                                                  int index, int ref,
                                                  int locked);
static int message_add_category_(struct msg_category *category);
static struct msg_category *message_del_category_(struct msg_category *category,
                                                  int index);
static void message_release_category(struct msg_category *self);

static int init_category_base(struct msg_category *p, const char *name,
                              int type);

static void close_multiplex_category(struct msg_category *self);
static void close_file_category(struct msg_category *self);
static void message_finalize(void);
static ssize_t multiplex_write(struct msg_category *self, int level,
                               const void *data, size_t size);
static ssize_t file_write(struct msg_category *self, int level,
                          const void *data, size_t size);
static int get_unix_tid(void);
static int prologue(char *buf, size_t size, const char *spec, int level,
                    const char *timef);

static unsigned long category_hash(const char *s);
static int get_level(const char *level);
static char *level_string(char *buf, size_t size, int level, int lower);

#ifdef HAVE_SYSLOG
static int parse_syslog_facility(const char *facility);
static int parse_syslog_option(const char *option);
static int syslog_priority(int level);
static ssize_t syslog_write(struct msg_category *self, int level,
                            const void *data, size_t size);
static void close_syslog_category(struct msg_category *self);
struct msg_category *message_syslog_category(const char *name,
                                             int option, int facility);
#endif  /* HAVE_SYSLOG */

static int run_signal_handler(void);
static void *signal_thread(void *arg);

#ifdef HAVE_JSON
static struct json_object *json_object_object_vget(struct json_object *obj,
                                                   va_list ap);
static struct json_object *json_object_object_xget(struct json_object *obj,
                                                   /* keys */ ...);
static int message_load_json(const char *pathname);

#endif  /* HAVE_JSON */


//static pthread_once_t message_init_once = PTHREAD_ONCE_INIT;
static pthread_key_t threadname_key;

static pthread_mutex_t cfg_mutex = PTHREAD_MUTEX_INITIALIZER;
static const char *cfg_pathname = NULL;

static pthread_mutex_t threadname_mutex = PTHREAD_MUTEX_INITIALIZER;
static int threadname_counter = 0;

static struct msg_cmap *category_map = NULL;
struct msg_category *MC_DEFAULT = NULL;

#define THREADNAME_MAX  64
#define TIMEBUF_MAX     256
#define PROLOGUE_MAX    1024


int
message_set_level(struct msg_category *category, int level)
{
  int ret;

  ret = WR_LOCK(&category->lock);
  if (ret) {
    errno = ret;
    return FALSE;
  }
  category->level = level;
  UNLOCK(&category->lock);
  return TRUE;
}

/*
 * Note that only producing functions/macros are thread-safe,
 * and category manipulating(add/remove/modify) functions are not.
 *
 * I'm not sure that reference counting on each catagory has a point.
 * If I cannot manipulating categories after initialization, why do I
 * need it?  Isn't it sufficient to provide removing all categories
 * for cleanup?  I guess so.
 *
 * For non-multiplexed category, once added into the category_map,
 * the category's refcount is zero.
 *
 * If the user called message_get_category(), the refcount increases
 * by one.  User need to call message_unget_category() once finished
 * to use it, which causes the refcount decrease by one.
 *
 * If the category is multiplxed by another category, the refcount is
 * increased by one.
 */
static __inline__ void
category_ref(struct msg_category *self, int locked)
{
  int ret;

  if (!locked) {
    ret = WR_LOCK(&self->lock);
    if (ret)
      fprintf(stderr, "error: locking failed: %s\n", strerror(ret));
    return;
  }
  self->refcount++;
  fprintf(stderr, "category %s: refcount = %d\n", self->name, self->refcount);
  if (!locked)
    UNLOCK(&self->lock);
}


#if 0
static __inline__ void
category_unref(struct msg_category *self)
{
  self->refcount--;

  if (self->refcount <= 0) {
    /* TODO: category->close?? */
    //self->close(self);
  }
}
#endif  /* 0 */


struct msg_cmap *
message_cmap_new(size_t size)
{
  struct msg_cmap *p;
  unsigned i, j;
  int ret;

  p = malloc(sizeof(*p));

  if (!p)
    return NULL;

  p->map = malloc(sizeof(*p->map) * size);
  if (!p->map)
    goto err;
  p->size = size;

  for (i = 0; i < size; i++)
    p->map[i] = NULL;

  if (pipe(p->pipefd))
    goto err_map;

  p->locks = malloc(sizeof(*p->locks) * size);
  if (!p->locks)
    goto err_pipe;

  for (i = 0; i < size; i++) {
    ret = INIT_LOCK(p->locks + i);
    if (ret) {
      for (j = 0; j < i; j++)
        DEL_LOCK(p->locks + j);
      goto err_locks;
    }
  }

  return p;

 err_locks:
  free(p->locks);
 err_pipe:
  close(p->pipefd[0]);
  close(p->pipefd[1]);

 err_map:
  free(p->map);
 err:
  free(p);
  return NULL;
}


/*
 * If LOCKED is FALSE, this function will force read-lock on category_map
 * for processing.
 *
 * If LOCKED is TRUE, this function does not force any lock on category_map.
 * (It's totally up to the caller to handle the lock.)
 */
static struct msg_category *
message_get_category_(const char *name, int index, int ref, int locked)
{
  struct msg_category *p;
  int ret;

  assert(category_map != NULL);

  if (index < 0)
    index = category_hash(name) % category_map->size;

  if (!locked) {
    ret = RD_LOCK(category_map->locks + index);
    if (ret)
      return NULL;
  }

  for (p = category_map->map[index]; p != NULL; p = p->next) {
    if (strcmp(name, p->name) == 0) {
      if (ref)
        category_ref(p, FALSE);
      if (!locked)
        UNLOCK(category_map->locks + index);
      return p;
    }
  }
  if (!locked)
    UNLOCK(category_map->locks + index);
  return NULL;
}


struct msg_category *
message_get_category(const char *name)
{
  return message_get_category_(name, -1, TRUE, FALSE);
}


void
message_unget_category(struct msg_category *category)
{
  int index;

  if (!category)
    return;

  index = category_hash(category->name) % category_map->size;

  WR_LOCK(&category->lock);

  if (category->refcount > 0) {
    category->refcount--;
    fprintf(stderr, "category %s: unget, refcount = %d\n",
            category->name, category->refcount);
    UNLOCK(&category->lock);
    return;
  }
  category->refcount--;
  fprintf(stderr, "category %s: unget(REMOVE), refcount = %d\n",
          category->name, category->refcount);

  message_del_category_(category, index);
  category->close(category);

  UNLOCK(&category->lock);
  message_release_category(category);
}


/*
 * This function extracts the struct CATEGORY from the category_map.
 * Note that this function does not REMOVE category itself.  Refer to
 * message_release_category() for this.
 */
struct msg_category *
message_del_category_(struct msg_category *category, int index)
{
  struct msg_category *p = category;

  assert(category_map != NULL);

  if (index < 0)
    index = category_hash(category->name) % category_map->size;

  if (!p)
    return FALSE;

  WR_LOCK(category_map->locks + index);

  if (p->prev)
    p->prev->next = p->next;
  if (p->next)
    p->next->prev = p->prev;

  if (category_map->map[index] == p)
    category_map->map[index] = p->next;

  UNLOCK(category_map->locks + index);

  // category_unref(p);
  /* TODO: category->close?? */

  return p;
}


/*
 * Add category into the category_map.
 *
 * Note that if there is msg_category with the same name in the map,
 * this function fails.
 */
static int
message_add_category_(struct msg_category *category)
{
  int index;
  struct msg_category *p;

  index = category_hash(category->name) % category_map->size;

  WR_LOCK(category_map->locks + index);
  p = message_get_category_(category->name, index, FALSE, TRUE);

  if (p) {
    UNLOCK(category_map->locks + index);
    return FALSE;
  }

  category->next = category_map->map[index];

  if (category->next)
    category->next->prev = category;

  category_map->map[index] = category;
  //category_ref(category);
  UNLOCK(category_map->locks + index);

  return TRUE;
}


static int
init_category_base(struct msg_category *p, const char *name, int type)
{
  int ret;

  p->next = p->prev = NULL;
  p->name = strdup(name);
  if (!p->name)
    return FALSE;

  p->type = type;
  p->level = MCL_WARN;
  //p->fd = -1;

  p->refcount = 0;

  p->fmt_prologue = NULL;
  p->fmt_time = NULL;

  p->write = NULL;
  p->close = NULL;

  ret = INIT_LOCK(&p->lock);
  if (ret) {
    errno = ret;
    return FALSE;
  }

  return TRUE;
}


static int
make_multiplex_room(struct category_multiplex *cp)
{
  struct msg_category **p;
  unsigned i;
  size_t newcap = cp->capacity;

  if (cp->size >= cp->capacity) {
    newcap += MC_MULTIPLEX_MAX;
    p = realloc(cp->sink, sizeof(*cp->sink) * newcap);
    if (!p)
      return FALSE;
    cp->capacity = newcap;
    cp->sink = p;

    for (i = cp->size; i < cp->capacity; i++)
      cp->sink[i] = NULL;
  }
  return TRUE;
}


int
message_multiplexer_add(struct msg_category *multiplexer,
                        struct msg_category *target)
{
  struct category_multiplex *cp = (struct category_multiplex *)multiplexer;
  int ret;

  fprintf(stderr, "multiplexer add %s into %s\n",
          target->name, multiplexer->name);
  if (!make_multiplex_room(cp))
    return FALSE;

  ret = WR_LOCK(&target->lock);
  if (ret) {
    fprintf(stderr, "warning: lock failed on %s\n", target->name);
    return FALSE;
  }
  cp->sink[cp->size++] = target;
  category_ref(target, TRUE);
  UNLOCK(&target->lock);

  return TRUE;
}


static ssize_t
multiplex_write(struct msg_category *self, int level,
                const void *data, size_t size)
{
  unsigned i;
  struct category_multiplex *p = (struct category_multiplex *)self;

  for (i = 0; i < p->size; i++) {
    if (p->sink[i]) {
      p->sink[i]->write(p->sink[i], level, data, size);
    }
  }
  return size;
}


static void
close_multiplex_category(struct msg_category *self)
{
  struct category_multiplex *cp = (struct category_multiplex *)self;

#if 0
  unsigned i;
  for (i = 0; i < cp->size; i++) {
    if (cp->sink[i]) {
      category_unref(cp->sink[i]);
    }
  }
#endif  /* 0 */

  free(cp->sink);
  cp->size = cp->capacity = 0;
  cp->sink = NULL;
}


struct msg_category *
message_multiplex_category(const char *name, ...)
{
  struct category_multiplex *cp;
  va_list ap;
  struct msg_category *child;

  cp = malloc(sizeof(*cp));
  if (!cp)
    return NULL;

  if (!init_category_base(&cp->base, name, MCT_MULTIPLEX)) {
    free(cp);
    return NULL;
  }
  //cp->base.fd = -1;
  cp->base.write = multiplex_write;
  cp->base.close = close_multiplex_category;
  cp->base.refresh = NULL;

  cp->size = cp->capacity = 0;
  cp->sink = NULL;
  if (!make_multiplex_room(cp)) {
    free(cp);
    return NULL;
  }

  va_start(ap, name);
  while ((child = va_arg(ap, struct msg_category *)) != NULL) {
    if (!message_multiplexer_add((struct msg_category *)cp, child)) {
      free(cp->sink);
      free(cp);
      return NULL;
    }
  }
  va_end(ap);

  if (!message_add_category_((struct msg_category *)cp)) {
    /* We failed to add the multiplxer category.  Possibly, before
     * adding, someone added another category with the same name.
     * Thus, unreference all child categories, then remove self. */
    unsigned i;

    for (i = 0; i < cp->size; i++) {
      message_unget_category(cp->sink[i]);
    }
    if (cp->base.refresh)
      cp->base.refresh((struct msg_category *)cp);

    message_release_category((struct msg_category *)cp);
    return NULL;
  }

  return (struct msg_category *)cp;
}


void
message_release_category(struct msg_category *self)
{
  free(self->name);
  free(self->fmt_prologue);
  free(self->fmt_time);
  DEL_LOCK(&self->lock);
  free(self);
}


static void
refresh_file_category(struct msg_category *self)
{
  struct category_file *cp = (struct category_file *)self;
  int old_fd, new_fd;

  new_fd = open(cp->pathname, FILE_MODE, FILE_ACC);

  fprintf(stderr, "resetting FD for %s\n", cp->pathname);

  //WR_LOCK(&cp->lock);
  old_fd = cp->fd;
  cp->fd = new_fd;
  //UNLOCK(&cp->lock);

  fsync(old_fd);
  close(old_fd);
}


static void
close_file_category(struct msg_category *self)
{
  struct category_file *p = (struct category_file *)self;

  if (p->fd >= 0) {
    fsync(p->fd);
    close(p->fd);
    p->fd = -1;
  }
  free(p->pathname);
}


static ssize_t
file_write(struct msg_category *self, int level, const void *data, size_t size)
{
  char prefix[PROLOGUE_MAX];
  char *msg = NULL;
  struct category_file *cp = (struct category_file *)self;
  int written = 0;

  (void)size;

  prologue(prefix, PROLOGUE_MAX, cp->base.fmt_prologue,
           level, cp->base.fmt_time);
  asprintf(&msg, "%s%s\n", prefix, (char *)data);

  //RD_LOCK(&cp->lock);
  written = write(cp->fd, msg, strlen(msg));
  //UNLOCK(&cp->lock);

  free(msg);

  return written;
}


struct msg_category *
message_file_category(const char *name, const char *pathname)
{
  struct category_file *cp;
  int saved_errno = 0;
  //int ret;

  assert(category_map != NULL);

  cp = malloc(sizeof(*cp));
  if (!cp)
    return NULL;

  if (!init_category_base(&cp->base, name, MCT_FILE)) {
    free(cp);
    return NULL;
  }
#if 0
  ret = INIT_LOCK(&cp->lock);
  if (ret) {
    saved_errno = ret;
    free(cp);
    errno = saved_errno;
    return NULL;
  }
#endif  /* 0 */

  if ((cp->pathname = strdup(pathname)) == NULL) {
    saved_errno = errno;
    free(cp);
    //DEL_LOCK(&cp->lock);
    errno = saved_errno;
    return NULL;
  }

  cp->fd = open(pathname, FILE_MODE, FILE_ACC);
  if (cp->fd == -1) {
    saved_errno = errno;
    free(cp->base.name);
    //DEL_LOCK(&cp->lock);
    free(cp);
    errno = saved_errno;
    return NULL;
  }

  cp->base.write = file_write;
  cp->base.close = close_file_category;
  cp->base.refresh = refresh_file_category;

  if (!message_add_category_((struct msg_category *)cp)) {
    cp->base.close((struct msg_category *)cp);
    message_release_category((struct msg_category *)cp);
    return NULL;
  }

  return (struct msg_category *)cp;
}


#ifdef HAVE_SYSLOG
static int
parse_syslog_facility(const char *facility)
{
  static struct pair {
    const char *name;
    int value;
  } logfacs[] = {
#define E_(x)   { #x, x }
    E_(LOG_AUTH),
    E_(LOG_AUTHPRIV),
    E_(LOG_CRON),
    E_(LOG_DAEMON),
    E_(LOG_FTP),
    //E_(LOG_KERN),
    E_(LOG_LOCAL0),
    E_(LOG_LOCAL1),
    E_(LOG_LOCAL2),
    E_(LOG_LOCAL3),
    E_(LOG_LOCAL4),
    E_(LOG_LOCAL5),
    E_(LOG_LOCAL6),
    E_(LOG_LOCAL7),
    E_(LOG_LPR),
    E_(LOG_MAIL),
    E_(LOG_NEWS),
#ifdef LOG_SECURITY
    E_(LOG_SECURITY),
#endif
    E_(LOG_SYSLOG),
    E_(LOG_USER),
    E_(LOG_UUCP),
#undef E_
  };
  unsigned i;

  if (!facility)
    return 0;

  for (i = 0; i < sizeof(logfacs) / sizeof(struct pair); i++) {
    if (strcmp(facility, logfacs[i].name) == 0)
      return logfacs[i].value;
  }
  return 0;
}


static int
parse_syslog_option(const char *option)
{
  static struct pair {
    const char *name;
    int value;
  } logopts[] = {
#define E_(x)   { #x, x }
    E_(LOG_CONS),
    E_(LOG_NDELAY),
#ifdef LOG_PERROR
    E_(LOG_PERROR),
#endif
#ifdef LOG_PID
    E_(LOG_PID),
#endif
#ifdef LOG_NOWAIT
    E_(LOG_NOWAIT),
#endif
#ifdef LOG_ODELAY
    E_(LOG_ODELAY),
#endif
#undef E_
  };
  unsigned i;

  if (!option)
    return 0;

  for (i = 0; i < sizeof(logopts) / sizeof(struct pair); i++) {
    if (strcmp(option, logopts[i].name) == 0)
      return logopts[i].value;
  }
  return 0;
}


static int
syslog_priority(int level)
{
  static int priorities[] = {
    LOG_CRIT,
    LOG_ERR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG,
  };

  if (level < 1)
    level = 1;
  if (level > MCL_DEBUG)
    level = MCL_DEBUG;

  return priorities[(level - 1) / 100];
}


static ssize_t
syslog_write(struct msg_category *self, int level,
             const void *data, size_t size)
{
  char prefix[PROLOGUE_MAX];
  char *msg = NULL;
  struct category_syslog *cp = (struct category_syslog *)self;

  if (cp->base.fmt_prologue) {
    prologue(prefix, PROLOGUE_MAX, cp->base.fmt_prologue,
             level, cp->base.fmt_time);
    asprintf(&msg, "%s%s", prefix, (char *)data);
    syslog(syslog_priority(level), "%s", (char *)msg);
    free(msg);
  }
  else
    syslog(syslog_priority(level), "%s", (char *)data);

  return size;
}


static void
close_syslog_category(struct msg_category *self)
{
  //struct category_syslog *p = (struct category_syslog *)self;
  (void)self;
  closelog();
}


struct msg_category *
message_syslog_category(const char *name, int option, int facility)
{
  struct category_syslog *cp;

  assert(category_map != NULL);

  if (facility == 0)
    facility = LOG_USER;

  cp = malloc(sizeof(*cp));
  if (!cp)
    return NULL;

  if (!init_category_base(&cp->base, name, MCT_SYSLOG)) {
    free(cp);
    return NULL;
  }

  cp->option = option;
  cp->facility = facility;

  openlog(name, option, facility);

  cp->base.write = syslog_write;
  cp->base.close = close_syslog_category;
  cp->base.refresh = NULL;

  if (!message_add_category_((struct msg_category *)cp)) {
    cp->base.close((struct msg_category *)cp);
    message_release_category((struct msg_category *)cp);

    return NULL;
  }

  return (struct msg_category *)cp;
}
#endif  /* HAVE_SYSLOG */


#if 0
static void
destroy_threadname(void *ptr)
{
  fprintf(stderr, "destroying threadname %s\n", (char *)ptr);
  free(ptr);
}
#endif  /* 0 */


/*
 * For the thread names, it is better to destory them in destroy
 * handler of pthread_key_create() rather than in clean-up handler
 * (established by pthread_cleanup_push).  If we embed destroying code
 * in the clean-up handler, because clean-up handlers are executed in
 * any order, we cannot use any message functions in the clean-up
 * handlers.  (I need to check if it is really working on the clean-up
 * handlers.)
 */
static void
message_init_(void)
{
  struct msg_category *p;
  const char *pathname = cfg_pathname;

  pthread_key_create(&threadname_key, free);
  //pthread_key_create(&threadname_key, destroy_threadname);

  category_map = message_cmap_new(MSG_CATEGORY_CMAP_SIZE);

  if (pathname) {
#ifdef HAVE_JSON
    if (!message_load_json(pathname)) {
      fprintf(stderr, "warning: error on loading configuration, \"%s\".\n",
              pathname);
    }
#endif
  }

  p = message_get_category_("default", -1, FALSE, FALSE);
  if (!p) {
    p = message_file_category("default", "/dev/stderr");
    if (!p) {
      fprintf(stderr, "message: message_init failed: %s\n",
              strerror(errno));
    }
  }
  MC_DEFAULT = p;

  atexit(message_finalize);
}


// message_init(pathname, flags, ...);
void
message_init(const char *pathname, ...)
//             void (*handler)(int, siginfo_t *, void *))
{
  va_list ap;
  int opt;
  void (*handler)(int, siginfo_t *, void *);
  struct sigaction act;
  int cont = TRUE;
  int ret = 0;

  va_start(ap, pathname);
  pthread_mutex_lock(&cfg_mutex);

  if (pathname && !cfg_pathname)
    cfg_pathname = strdup(pathname);
  //ret = pthread_once(&message_init_once, message_init_);
  message_init_();

  while (cont) {
    opt = (int)va_arg(ap, int);

    switch (opt) {
    case MCO_SIGHUP:
      handler = (void (*)(int, siginfo_t *, void *)) \
        va_arg(ap, void (*)(int, siginfo_t *, void *));
      run_signal_handler();
      category_map->alt_huphandler = handler;

      memset(&act, 0, sizeof(act));

      act.sa_sigaction = message_signal_handler;
      sigfillset(&act.sa_mask);
      act.sa_flags = SA_SIGINFO;

      if (sigaction(SIGHUP, &act, NULL) == -1) {
        fprintf(stderr, "sigaction(2) failed: %s\n", strerror(errno));
      }
      break;
    case MCO_NONE:
    default:
      cont = FALSE;
      break;
    }
  }
  pthread_mutex_unlock(&cfg_mutex);

  if (ret) {
    fprintf(stderr, "message: message_init() failed\n");
    abort();
  }
}


static void
message_finalize(void)
{
  unsigned i;
  struct msg_cmap *dst = category_map;
  struct msg_category *cp;
  category_map = NULL;

  for (i = 0; i < dst->size; i++)
    WR_LOCK(dst->locks + i);

  for (i = 0; i < dst->size; i++) {
    while (dst->map[i]) {
      cp = dst->map[i];
      dst->map[i] = cp->next;

      if (cp->close)
        cp->close(cp);
      message_release_category(cp);
    }
  }

  for (i = 0; i < dst->size; i++) {
    UNLOCK(dst->locks + i);
    DEL_LOCK(dst->locks + i);
  }

  free(dst->locks);
  free(dst->map);
  free(dst);

  free((void *)cfg_pathname);
}


void
message_thread_init(const char *thread_name)
{
  char buf[64];
  int seq;

  if (thread_name)
    pthread_setspecific(threadname_key, strdup(thread_name));
  else {
    pthread_mutex_lock(&threadname_mutex);
    seq = threadname_counter;
    seq++;
    pthread_mutex_unlock(&threadname_mutex);

    snprintf(buf, THREADNAME_MAX, "thread#%u", seq);
    pthread_setspecific(threadname_key, strdup(buf));
  }
}


static char *
level_string(char *buf, size_t size, int level, int lower)
{
  static struct isstuple {
    int lev;
    char *names[2];
  } levels[] = {
    { MCL_FATAL, { "FATAL", "fatal" } },
    { MCL_ERR,   { "ERR",   "err" } },
    { MCL_WARN,  { "WARN",  "warn" } },
    { MCL_INFO,  { "INFO",  "info" } },
    { MCL_DEBUG, { "DEBUG", "debug" } },
  };
  assert(level >= 0 && level <= MCL_DEBUG);

  lower = !!lower;

  if (level % 100) {
    //snprintf(buf, size, "%s%+d",
    snprintf(buf, size, "%s%+d", levels[(level - 1) / 100].names[lower],
             level - levels[(level - 1) / 100].lev);
  }
  else {
    snprintf(buf, size, "%s", levels[(level - 1) / 100].names[lower]);
  }

#if 0
  if (level > MCL_DEBUG)
    snprintf(buf, size, "DEBUG+%d", level - MCL_DEBUG);
  else if (level == MCL_DEBUG)
    snprintf(buf, size, "DEBUG");
  else if (level > MCL_INFO)
    snprintf(buf, size, "INFO+%d", level - MCL_INFO);
  else if (level == MCL_INFO)
    snprintf(buf, size, "INFO");
  else if (level > MCL_WARN)
    snprintf(buf, size, "WARN+%d", level - MCL_WARN);
  else if (level == MCL_WARN)
    snprintf(buf, size, "WARN");
  else if (level > MCL_ERR)
    snprintf(buf, size, "ERR+%d", level - MCL_ERR);
  else if (level == MCL_ERR)
    snprintf(buf, size, "ERR");
  else if (level > MCL_FATAL)
    snprintf(buf, size, "FATAL+%d", level - MCL_FATAL);
  else if (level == MCL_FATAL)
    snprintf(buf, size, "FATAL");
  else
    snprintf(buf, size, "FATAL-%d", MCL_FATAL - level);
#endif  /* 0 */

  return buf;
}


/*
   %20P - process id
   %10T - thread id
   %l - level (lowercase)
   %L - level (uppercase)
   %t - local time
   %T - UTC time
 */
static int
prologue(char *buf, size_t size, const char *spec, int level, const char *timef)
{
  const char *p = spec;
  char *q, *end, *endptr;
  int ret;
  int width = 0;
  size_t wrote = 0;
  char timebuf[TIMEBUF_MAX];

  q = buf;

  end = buf + size;

  if (spec) {
    while (*p && q < end) {
      if (*p == '%') {
        p++;

        width = strtol(p, &endptr, 10);
        if (endptr == p)          /* no number after '%' */
          width = 0;
        else
          p = endptr;

        switch (*p) {
        case 'P':
          ret = snprintf(q, end - q, "%*u", width, (unsigned)getpid());
          if (ret >= end - q) {
            /* buffer overflow! */
            q = end - 1;
            wrote = q - buf;

            goto end_;
          }
          else {
            q += ret;
            wrote += ret;
          }
          break;
        case 'h':
          {
            const char *name;
            int tid;

            tid = get_unix_tid();
            if (tid == -1) {
              name = pthread_getspecific(threadname_key);
              ret = snprintf(q, end - q, "%*s", width, name ? name : "");
            }
            else
              ret = snprintf(q, end - q, "%*u", width, tid);

            if (ret >= end - q) {
              /* buffer overflow! */
              q = end - 1;
              wrote = q - buf;

              goto end_;
            }
            else {
              q += ret;
              wrote += ret;
            }
          }
          break;
        case 'l':               /* level */
        case 'L':
          {
            level_string(timebuf, TIMEBUF_MAX, level, *p == 'L' ? 0 : 1);
            ret = snprintf(q, end - q, "%*s", width, timebuf);
            if (ret >= end - q) {
              /* buffer overflow! */
              q = end - 1;
              wrote = q - buf;

              goto end_;
            }
            else {
              q += ret;
              wrote += ret;
            }
          }

          break;

        case 't':
        case 'T':
          {
            time_t now;
            struct tm tmbuf;

            time(&now);

            if (*p == 't')
              localtime_r(&now, &tmbuf);
            else
              gmtime_r(&now, &tmbuf);
            strftime(timebuf, TIMEBUF_MAX, timef, &tmbuf);

            ret = snprintf(q, end - q, "%*s", width, timebuf);
            if (ret >= end - q) {
              /* buffer overflow! */
              q = end - 1;
              wrote = q - buf;

              goto end_;
            }
            else {
              q += ret;
              wrote += ret;
            }
          }

        default:
          break;
        }
        p++;
      }
      else {
        *q++ = *p++;
        wrote++;
      }
    }
  }
 end_:
  *q = '\0';

  return wrote;
}


void
message_c(struct msg_category *category, int level, const char *fmt, ...)
{
  va_list ap;
  char *user = NULL;
  int sz;
  int ret;

  if (category == NULL)
    return;

  ret = RD_LOCK(&category->lock);
  if (ret)
    return;

  if (level > category->level) {
    UNLOCK(&category->lock);
    return;
  }

  va_start(ap, fmt);
  sz = vasprintf(&user, fmt, ap);
  va_end(ap);

  category->write(category, level, user, sz);
  UNLOCK(&category->lock);

  free(user);
}


static unsigned long
category_hash(const char *s)
{
  /* Stealed from GNU glib, g_string_hash function */
  const char *p = s;
  int len = strlen(s);
  unsigned long h = 0;

  while (len--) {
    h = (h << 5) - h + *p;
    p++;
  }

  return h;
}


static int
get_unix_tid(void)
{
  int ret = -1;

#ifdef linux
  ret = syscall(SYS_gettid);
#elif defined(__sun)
  ret = pthread_self();
#elif defined(__APPLE__)
  ret = mach_thread_self();
  mach_port_deallocate(mach_task_self(), ret);
#elif defined(__FreeBSD__)
  long lwpid;
  thr_self(&lwpid);
  ret = lwpid;
#elif defined(__DragonFly__)
  ret = lwp_gettid();
#endif

  return ret;
}


static int
get_level(const char *level)
{
  char *endptr = NULL;
  int val;

  if (!level)
    return MCL_WARN;

  val = strtol(level, &endptr, 0);
  if (*endptr == '\0')
    return val;

  if (strncasecmp(level, "debug", 5) == 0)
    return MCL_DEBUG;
  if (strncasecmp(level, "info", 4) == 0)
    return MCL_INFO;
  if (strncasecmp(level, "warn", 4) == 0)
    return MCL_WARN;
  if (strncasecmp(level, "err", 3) == 0)
    return MCL_ERR;
  if (strncasecmp(level, "fatal", 5) == 0)
    return MCL_FATAL;

  return MCL_WARN;
}


#ifdef HAVE_JSON
static struct json_object *
json_object_object_vget(struct json_object *obj, va_list ap)
{
  const char *key;

  while ((key = (const char *)va_arg(ap, const char *)) != NULL) {
    if (json_object_get_type(obj) != json_type_object)
      return NULL;
    obj = json_object_object_get(obj, key);
    if (!obj)
      return NULL;
  }
  return obj;
}


static struct json_object *
json_object_object_xget(struct json_object *obj, /* keys */ ...)
{
  va_list ap;

  va_start(ap, obj);
  obj = json_object_object_vget(obj, ap);
  va_end(ap);

  return obj;
}


static int
message_load_json(const char *pathname)
{
  struct msg_category *mc;
  struct json_object *conf;
  struct json_object *root = json_object_from_file((char *)pathname);

  if (!root || root == (struct json_object *)-1)
    return FALSE;

  conf = json_object_object_xget(root, "message", NULL);
  if (!conf) {
    json_object_put(root);
    return FALSE;
  }

  {
    json_object_object_foreach(conf, key, val) {
      // printf("key: %s\n", key);
      {
        struct json_object *type = json_object_object_get(val, "type");

        if (!type) {
          fprintf(stderr, "warning: category %s does not have type field.\n",
                  key);
          continue;
        }

        if (strcmp(json_object_get_string(type), "file") == 0) {
          struct json_object *path = json_object_object_get(val, "pathname");
          struct json_object *level = json_object_object_get(val, "level");
          struct json_object *tm = json_object_object_get(val, "time");
          struct json_object *pr = json_object_object_get(val, "prologue");

          if (!path) {
            fprintf(stderr,
                    "warning: category %s does not have pathname field.\n",
                    key);
            continue;
          }
          mc = message_file_category(key, json_object_get_string(path));

          if (level)
            mc->level = get_level(json_object_get_string(level));

          if (tm)
            mc->fmt_time = strdup(json_object_get_string(tm));

          if (pr)
            mc->fmt_prologue = strdup(json_object_get_string(pr));

        }
#ifdef HAVE_SYSLOG
        else if (strcmp(json_object_get_string(type), "syslog") == 0) {
          struct msg_category *mc;
          struct json_object *opts = json_object_object_get(val, "option");
          struct json_object *fac = json_object_object_get(val, "facility");
          struct json_object *level = json_object_object_get(val, "level");
          struct json_object *tm = json_object_object_get(val, "time");
          struct json_object *pr = json_object_object_get(val, "prologue");
          int i, size;
          int options = 0;
          int facility = 0;

          if (opts) {
            size = json_object_array_length(opts);
            for (i = 0; i < size; i++) {
              struct json_object *o = json_object_array_get_idx(opts, i);
              options |= parse_syslog_option(json_object_get_string(o));
            }
          }

          facility = parse_syslog_facility(json_object_get_string(fac));
          mc = message_syslog_category(key, options, facility);

          if (level)
            mc->level = get_level(json_object_get_string(level));

          if (tm)
            mc->fmt_time = strdup(json_object_get_string(tm));

          if (pr)
            mc->fmt_prologue = strdup(json_object_get_string(pr));
        }
#endif  /* HAVE_SYSLOG */
      }
    }
  }

  {
    json_object_object_foreach(conf, key, val) {
      {
        struct json_object *type = json_object_object_get(val, "type");

        if (!type) {
          continue;
        }

        if (strcmp(json_object_get_string(type), "multiplex") == 0) {
          struct json_object *childs = json_object_object_get(val, "children");
          struct json_object *level = json_object_object_get(val, "level");
          struct json_object *tm = json_object_object_get(val, "time");
          struct json_object *pr = json_object_object_get(val, "prologue");

          //struct array_list *ar = json_object_get_array(childs);
          int i, size;
          struct msg_category *mux, *mc;

          if (!json_object_is_type(childs, json_type_array)) {
            fprintf(stderr, "error: \"children\" should be an array type.\n");
            goto end;
          }
          else {
            size = json_object_array_length(childs);
            mux = message_multiplex_category(key, NULL);

            for (i = 0; i < size; i++) {
              struct json_object *ch = json_object_array_get_idx(childs, i);

              mc = message_get_category_(json_object_get_string(ch), -1,
                                         FALSE, FALSE);
              if (!mc) {
                fprintf(stderr, "warning: category %s is not defined\n",
                        json_object_get_string(ch));
                continue;
              }
              message_multiplexer_add(mux, mc);
            }
            if (level)
              mux->level = get_level(json_object_get_string(level));

            if (tm)
              mux->fmt_time = strdup(json_object_get_string(tm));

            if (pr)
              mux->fmt_prologue = strdup(json_object_get_string(pr));
          }
        }
      }
    }
  }
 end:
  json_object_put(root);
  return TRUE;
}
#endif  /* HAVE_JSON */


void
message_signal_handler(int signo, siginfo_t *info, void *uap)
{
  static char buf[1] = { '!' };

  if (category_map) {
    write(category_map->pipefd[1], buf, sizeof(buf));

    if (category_map->alt_huphandler)
      category_map->alt_huphandler(signo, info, uap);
  }
}


static int
run_signal_handler(void)
{
  sigset_t set, oset;
  int ret;
  pthread_attr_t attr;

  sigfillset(&set);
  //sigdelset(&set, SIGHUP);
  sigemptyset(&oset);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  pthread_sigmask(SIG_SETMASK, &set, &oset);

  ret = pthread_create(&category_map->sigthread, &attr, signal_thread,
                       (void *)&category_map->pipefd[0]);
  if (ret) {
    fprintf(stderr, "error: pthread_create() failed: %s\n",
            strerror(errno));
  }
  pthread_sigmask(SIG_SETMASK, &oset, NULL);

  pthread_attr_destroy(&attr);
  return !ret;
}


static void *
signal_thread(void *arg)
{
  char buf[PIPE_BUF];
  int readch;
  unsigned i, sz;
  struct msg_category *mc;
  int fd = *(int *)arg;

  while (1) {
    readch = read(fd, buf, PIPE_BUF);

    if (readch == -1) {
      break;
    }

    sz = category_map->size;

    for (i = 0; i < sz; i++) {
      for (mc = category_map->map[i]; mc != NULL; mc = mc->next) {
        WR_LOCK(&mc->lock);
        if (mc->refresh)
          mc->refresh(mc);
        UNLOCK(&mc->lock);
      }
    }
  }

  return NULL;
}


#ifdef _TEST_JSON
int
main(int argc, char *argv[])
{

  message_init(argv[1]);

  {
    struct msg_category *mc;
    mc = message_get_category("multi");

    message_c(mc, 0, MCL_INFO + 3, "hello custom level");
    message_c(mc, 0, -5, "hello custom level");

    c_debug(mc, "hello world");
    c_err(mc, "hello world");
    c_fatal(mc, "hello world");
    c_warn(mc, "hello world");

    /* ... */
  }

  return 0;
}
#endif  /* _TEST_JSON */

#ifdef _TEST_YAML
#include <yaml.h>

struct sspair {
  char *name;
  char *value;
};

struct sipair {
  int val;
  const char *name;
} token_types[] = {
#define P_(x)   { x, #x }
  P_(YAML_NO_TOKEN),
  P_(YAML_STREAM_START_TOKEN),
  P_(YAML_STREAM_END_TOKEN),
  P_(YAML_VERSION_DIRECTIVE_TOKEN),
  P_(YAML_TAG_DIRECTIVE_TOKEN),
  P_(YAML_DOCUMENT_START_TOKEN),
  P_(YAML_DOCUMENT_END_TOKEN),
  P_(YAML_BLOCK_SEQUENCE_START_TOKEN),
  P_(YAML_BLOCK_MAPPING_START_TOKEN),
  P_(YAML_BLOCK_END_TOKEN),
  P_(YAML_FLOW_SEQUENCE_START_TOKEN),
  P_(YAML_FLOW_SEQUENCE_END_TOKEN),
  P_(YAML_FLOW_MAPPING_START_TOKEN),
  P_(YAML_FLOW_MAPPING_END_TOKEN),
  P_(YAML_BLOCK_ENTRY_TOKEN),
  P_(YAML_FLOW_ENTRY_TOKEN),
  P_(YAML_KEY_TOKEN),
  P_(YAML_VALUE_TOKEN),
  P_(YAML_ALIAS_TOKEN),
  P_(YAML_ANCHOR_TOKEN),
  P_(YAML_TAG_TOKEN),
  P_(YAML_SCALAR_TOKEN),
};


static const char *
token_name(int type)
{
  int i;

  for (i = 0; i < sizeof(token_types) / sizeof(struct sipair); i++) {
    if (token_types[i].val == type)
      return token_types[i].name;
  }
  return "*UNKNOWN*";
}


int
message_load_yaml(const char *pathname)
{
  yaml_parser_t parser;
  yaml_event_t event;
  FILE *fp;
  int done = 0;
  yaml_document_t doc;
  int level = 0;

  int do_message = FALSE;
  int true_on_key = TRUE;

  char *name_c = NULL;

  fp = fopen(pathname, "r");
  if (!fp)
    return FALSE;

  yaml_parser_initialize(&parser);
  yaml_parser_set_input_file(&parser, fp);

#if 0
  while (!done) {
    if (!yaml_parser_parse(&parser, &event))
      break;
#if 0
    printf("%s(%d):\n", token_name(token.type), token.type);

    if (token.type == YAML_SCALAR_TOKEN)
      printf("\tscalar: %s\n", token.data.scalar.value);
#endif  /* 0 */

    switch (event.type) {
    case YAML_SEQUENCE_START_EVENT:
      printf("seq start\n");
      level++;
      break;
    case YAML_SEQUENCE_END_EVENT:
      printf("seq start\n");
      level--;
      break;
    case YAML_MAPPING_START_EVENT:
      printf("map start\n");
      level++;
      break;
    case YAML_MAPPING_END_EVENT:
      printf("map start\n");
      level--;
      break;

    case YAML_ALIAS_EVENT:
      printf("%*sALIAS: %s (%d)\n", level << 2, " ", event.data.alias.anchor, level);
      break;

    case YAML_SCALAR_EVENT:
      printf("%*sSCALAR: %s (%d)\n", level << 2, " ",
             event.data.scalar.value, level);

      if (level == 1 && strcmp(event.data.scalar.value, "message") == 0)
        do_message = TRUE;
      else if (level == 2 && do_message) {
        name_c = strdup(event.data.scalar.value);
      }

      break;
    default:
      break;
    }

    done = (event.type == YAML_STREAM_END_EVENT);
    yaml_event_delete(&event);
  }
#endif  /* 0 */
  yaml_parser_load(&parser, &doc);


  yaml_parser_delete(&parser);
  fclose(fp);
}


int
main(int argc, char *argv[])
{
  char *buf;
  int size = 50;
  int ret;

  message_init();
  message_thread_init(NULL);

  printf("PIPE_BUF: %d\n", PIPE_BUF);

  buf = malloc(size);
  ret = prologue(buf, size, "|%20t%10P:%h|", "%FT%T");
  printf("ret: %d\nbuf: %s\n", ret, buf);
  free(buf);

  message_load_yaml(argv[1]);
  return 0;
}


#endif  /* _TEST */
