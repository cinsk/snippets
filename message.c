#define _GNU_SOURCE

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

#ifdef HAVE_JSON
#define __STRICT_ANSI__
#include <json/json.h>
#undef __STRICT_ANSI__
#endif  /* HAVE_JSON */

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

#define MCF_UTC 0x0001

struct msg_cmap {
  size_t size;
  struct msg_category **map;
};

#define MSG_CATEGORY_CMAP_SIZE  509

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

static struct msg_category *message_get_category_(const char *name, int index);
static int message_add_category_(struct msg_category *category);
static int message_del_category_(struct msg_category *category, int index);

static int init_category_base(struct msg_category *p,
                              const char *name, int type);
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

#ifdef HAVE_JSON
static struct json_object *json_object_object_vget(struct json_object *obj,
                                                   va_list ap);
static struct json_object *json_object_object_xget(struct json_object *obj,
                                                   /* keys */ ...);
static int message_load_json(const char *pathname);

#endif  /* HAVE_JSON */


static pthread_once_t message_init_once = PTHREAD_ONCE_INIT;
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

/*
 * Note that only producing functions/macros are thread-safe,
 * and category manipulating(add/remove/modify) functions are not.
 *
 * I'm not sure that reference counting on each catagory has a point.
 * If I cannot manipulating categories after initialization, why do I
 * need it?  Isn't it sufficient to provide removing all categories
 * for cleanup?  I guess so.
 */
static __inline__ void
category_ref(struct msg_category *self)
{
  self->refcount++;
}


static __inline__ void
category_unref(struct msg_category *self)
{
  self->refcount--;

  if (self->refcount <= 0) {
    /* TODO: category->close?? */
    //self->close(self);
  }
}


struct msg_cmap *
message_cmap_new(size_t size)
{
  struct msg_cmap *p;
  unsigned i;

  p = malloc(sizeof(*p));

  if (!p)
    return NULL;

  p->map = malloc(sizeof(*p->map) * size);
  if (!p->map) {
    free(p);
    return NULL;
  }
  p->size = size;

  for (i = 0; i < size; i++)
    p->map[i] = NULL;

  return p;
}


static struct msg_category *
message_get_category_(const char *name, int index)
{
  struct msg_category *p;

  assert(category_map != NULL);

  if (index < 0)
    index = category_hash(name) % category_map->size;

  for (p = category_map->map[index]; p != NULL; p = p->next) {
    if (strcmp(name, p->name) == 0)
      return p;
  }
  return NULL;
}


struct msg_category *
message_get_category(const char *name)
{
  return message_get_category_(name, -1);
}


int
message_del_category_(struct msg_category *category, int index)
{
  struct msg_category *p = category;

  assert(category_map != NULL);

  if (index < 0)
    index = category_hash(category->name) % category_map->size;

  if (!p)
    return FALSE;

  if (p->prev)
    p->prev->next = p->next;
  if (p->next)
    p->next->prev = p->prev;

  if (category_map->map[index] == p)
    category_map->map[index] = p->next;

  category_unref(p);
  /* TODO: category->close?? */

  return TRUE;
}


static int
message_add_category_(struct msg_category *category)
{
  int index;
  struct msg_category *p;

  index = category_hash(category->name) % category_map->size;

  p = message_get_category_(category->name, index);

  if (p)
    message_del_category_(p, index);

  category->next = category_map->map[index];

  if (category->next)
    category->next->prev = category;

  category_map->map[index] = category;
  category_ref(category);

  return TRUE;
}


static int
init_category_base(struct msg_category *p, const char *name, int type)
{
  p->next = p->prev = NULL;
  p->name = strdup(name);
  if (!p->name)
    return FALSE;

  p->type = type;
  p->level = MCL_WARN;
  //p->fd = -1;

  p->flags = 0;
  p->refcount = 0;

  p->fmt_prologue = NULL;
  p->fmt_time = NULL;

  p->write = NULL;
  p->close = NULL;

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

  if (!make_multiplex_room(cp))
    return FALSE;

  cp->sink[cp->size++] = target;
  category_ref(target);

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

  message_close_category(self);
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

  message_add_category_((struct msg_category *)cp);

  return (struct msg_category *)cp;
}


void
message_close_category(struct msg_category *self)
{
  free(self->name);
  free(self->fmt_prologue);
  free(self->fmt_time);
  free(self);
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
  message_close_category(self);
}


static ssize_t
file_write(struct msg_category *self, int level, const void *data, size_t size)
{
  char prefix[PROLOGUE_MAX];
  char *msg = NULL;
  struct category_file *cp = (struct category_file *)self;
  int written = 0;

  prologue(prefix, PROLOGUE_MAX, cp->base.fmt_prologue,
           level, cp->base.fmt_time);
  asprintf(&msg, "%s%s\n", prefix, (char *)data);
  written = write(cp->fd, msg, strlen(msg));
  free(msg);

  return written;
}


struct msg_category *
message_file_category(const char *name, const char *pathname)
{
  struct category_file *cp;
  int saved_errno = 0;

  assert(category_map != NULL);

  cp = malloc(sizeof(*cp));
  if (!cp)
    return NULL;

  if (!init_category_base(&cp->base, name, MCT_FILE)) {
    free(cp);
    return NULL;
  }

  if ((cp->pathname = strdup(pathname)) == NULL) {
    saved_errno = errno;
    free(cp);
    errno = saved_errno;
    return NULL;
  }

  cp->fd = open(pathname, O_CREAT | O_APPEND | O_CLOEXEC | O_WRONLY,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (cp->fd == -1) {
    saved_errno = errno;
    free(cp->base.name);
    free(cp);
    errno = saved_errno;
    return NULL;
  }

  cp->base.write = file_write;
  cp->base.close = close_file_category;

  message_add_category_((struct msg_category *)cp);

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
  int i;

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
  int i;

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

  closelog();
  message_close_category(self);
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

  message_add_category_((struct msg_category *)cp);

  return (struct msg_category *)cp;
}
#endif  /* HAVE_SYSLOG */


static void
message_init_(void)
{
  struct msg_category *p;
  const char *pathname = cfg_pathname;

  pthread_key_create(&threadname_key, free);

  category_map = message_cmap_new(MSG_CATEGORY_CMAP_SIZE);

  if (pathname) {
#ifdef HAVE_JSON
    message_load_json(pathname);
#endif
  }

  p = message_get_category_("default", -1);
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


void
message_init(const char *pathname)
{
  int ret = 0;

  pthread_mutex_lock(&cfg_mutex);
  if (pathname && !cfg_pathname)
    cfg_pathname = strdup(pathname);
  //ret = pthread_once(&message_init_once, message_init_);
  message_init_();
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

  for (i = 0; i < dst->size; i++) {
    while (dst->map[i]) {
      cp = dst->map[i];
      dst->map[i] = cp->next;

      if (cp->close)
        cp->close(cp);
      else
        message_close_category(cp);
    }
  }

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

  if (category == NULL)
    return;

  if (level > category->level)
    return;

  va_start(ap, fmt);
  sz = vasprintf(&user, fmt, ap);
  va_end(ap);

  category->write(category, level, user, sz);

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
  if (!root)
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
          int size = json_object_array_length(childs);
          int i;
          struct msg_category *mux, *mc;

          mux = message_multiplex_category(key, NULL);

          for (i = 0; i < size; i++) {
            struct json_object *ch = json_object_array_get_idx(childs, i);
            //printf("array[%d] = %s\n", i, (char *)json_object_get_string(ch));
            mc = message_get_category(json_object_get_string(ch));
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
  json_object_put(root);
  return TRUE;
}
#endif  /* HAVE_JSON */


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
