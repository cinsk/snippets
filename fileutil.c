#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>

#define DIR_SEPARATOR   '/'
#define DIR_SEPARATORS   "/"


#define DIRSTACK_CHUNK_SIZE     256

#define ERROR_GOTO(error, retcode, label)       do {      \
  if (error) { saved_errno = errno; } \
  ret = (retcode);                    \
  goto label;                         \
  } while (0)

struct dirstack {
  char **data;
  size_t size;
  size_t current;
};

/*
 * A set of functions for the directory(pathname) stack.
 *
 * ds_init() to initialize the directory stack,
 * ds_free() to release the directory stack,
 * ds_push() to push new pathname (with malloc) into the stack,
 * ds_push_noalloc() to push new pathname (without malloc) into the stack,
 * ds_pop() to pop the pathname from the stack (you need to free),
 * ds_push_join() to push new concatenated pathname from DIR and ENT.
 */
static void ds_init(struct dirstack *q);
static int ds_incr(struct dirstack *q);
static void ds_free(struct dirstack *q);
static int ds_push_noalloc(struct dirstack *q, const char *dir);
static int ds_push(struct dirstack *q, const char *dir);
static char *ds_pop(struct dirstack *q);
static int ds_push_join(struct dirstack *q, const char *dir, const char *ent);

const char *
filename_extension_imm(const char *pathname)
{
  size_t len = strlen(pathname);
  const char *dirp, *dotp;

  dirp = strrchr(pathname, DIR_SEPARATOR);
  dotp = strrchr(pathname, '.');

  if (!dotp)
    return pathname + len;

  if (dotp > dirp + 1)
    return dotp + 1;

  return pathname + len;
}


const char *
filename_basename_imm(const char *pathname)
{
  size_t len = strlen(pathname);
  const char *p;

  if (strcmp(pathname, DIR_SEPARATORS) == 0)
    return pathname;

  for (p = pathname + len - 1; p >= pathname; p--) {
    if (*p == DIR_SEPARATOR)
      return p + 1;
  }
  return p;
}


char *
filename_directory(char *pathname)
{
  size_t len = strlen(pathname);
  char *p = pathname + len;

  while (p >= pathname) {
    if (*p == DIR_SEPARATOR) {
      *p = '\0';
      break;
    }
    p--;
  }

  if (p < pathname) {           /* e.g. pathname = "asdf" */
    strcpy(pathname, ".");
  }
  else if (p == pathname) {     /* e.g. pathname = "/asdf" */
    strcpy(pathname, "/");
  }
  return pathname;
}


char *
filename_join(const char *dirname, const char *filename)
{
  size_t len, sum;
  char *p, *q;

  assert(dirname != NULL || filename != NULL);

  if (dirname == NULL)
    return strdup(filename);
  if (filename == NULL)
    return strdup(dirname);

  len = strlen(dirname);
  //len2 = strlen(filename);

  sum = len;
  if (dirname[len - 1] != DIR_SEPARATOR)
    sum++;
  filename += strspn(filename, DIR_SEPARATORS);
  sum += strlen(filename);

  p = malloc(sum + 1);
  strcpy(p, dirname);
  q = p + len;
  if (*(q - 1) != DIR_SEPARATOR)
    *q++ = DIR_SEPARATOR;
  strcpy(q, filename);

  return p;
}


static void
ds_init(struct dirstack *q)
{
  q->data = NULL;
  q->size = 0;
  q->current = 0;
}


static int
ds_incr(struct dirstack *q)
{
  size_t newsize = q->size + DIRSTACK_CHUNK_SIZE;
  char **p;

  if (q->current >= q->size) {
    p = realloc(q->data, sizeof(*q->data) * newsize);
    if (!p)
      return -1;
    q->data = p;
    q->size = newsize;
  }
  return 0;
}


static void
ds_free(struct dirstack *q)
{
  int i;
  for (i = 0; i < q->current; i++)
    free(q->data[i]);
  free(q->data);
}


static int
ds_push_noalloc(struct dirstack *q, const char *dir)
{
  char *p;

  if (ds_incr(q) < 0)
    return -1;

  p = (char *)dir;
  if (!p)
    return -1;

  q->data[q->current++] = p;
  return 0;
}


static int
ds_push(struct dirstack *q, const char *dir)
{
  int ret;
  char *p;

  p = strdup(dir);
  if (!p)
    return -1;
  ret = ds_push_noalloc(q, p);
  if (ret < 0)
    free(p);
  return ret;
}


static char *
ds_pop(struct dirstack *q)
{
  if (q->current > 0)
    return q->data[--q->current];
  else
    return NULL;
}


static int
ds_push_join(struct dirstack *q, const char *dir, const char *ent)
{
  return ds_push_noalloc(q, filename_join(dir, ent));
}


int
delete_directory(const char *pathname, int force)
{
  DIR *dir = NULL;
  struct dirent *ent;
  char *base, *name;
  struct stat statbuf;
  struct dirstack stack;
  int ret = 0;
  int saved_errno = errno;

  ds_init(&stack);
  ds_push(&stack, pathname);

  while ((base = ds_pop(&stack)) != NULL) {
    if (force) {
      if (lstat(base, &statbuf) < 0)
        ERROR_GOTO(errno, -1, fin);
      if (chmod(base, (statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) \
                | S_IRUSR | S_IWUSR | S_IXUSR) < 0)
        ERROR_GOTO(errno, -1, fin);
    }
    dir = opendir(base);
    if (!dir) {
      fprintf(stderr, "\terrno = %d, %s", errno, strerror(errno));
      ERROR_GOTO(errno, -1, fin);
    }

    while ((ent = readdir(dir)) != NULL) {
      if (strcmp(ent->d_name, ".") == 0 ||
          strcmp(ent->d_name, "..") == 0)
        continue;

      name = filename_join(base, ent->d_name);

      if (lstat(name, &statbuf) < 0) {
        /* TODO: now what? */
        free(name);
        closedir(dir);
        fprintf(stderr, "\terrno = %d, %s", errno, strerror(errno));
        ERROR_GOTO(errno, -1, fin);
      }

      if (S_ISDIR(statbuf.st_mode)) {
        closedir(dir);
        ds_push_noalloc(&stack, base);
        ds_push_noalloc(&stack, name);
        break;
      }
      else {
        fprintf(stderr, "UNLINK %s\n", name);
        if (unlink(name) < 0) {
          fprintf(stderr, "\terrno = %d, %s", errno, strerror(errno));
          saved_errno = errno;
          ret = -1;
        }
        free(name);
      }
    }
    if (ent == NULL) {
      /* Nothing to do with ENT */
      closedir(dir);
      fprintf(stderr, "RMDIR |%s|\n", base);
      if (rmdir(base) < 0) {
        fprintf(stderr, "\terrno = %d, %s", errno, strerror(errno));
        saved_errno = errno;
        ret = -1;
      }
      free(base);
    }
    else {
      /* There is suspended work in this ENT. */
    }
  }

 fin:
  ds_free(&stack);
  errno = saved_errno;
  return ret;
}


int
make_directory(const char *pathname)
{
  char *path;
  size_t len;
  char *p;
  mode_t mask;
  char saved_char;
  int ret = -1;
  int saved_errno = errno;

  if (!pathname)
    return -1;

  len = strlen(pathname);
  if (len < 1)
    len = 1;
  path = malloc(len + 1);
  if (!path)
    return -1;
  strcpy(path, pathname);

  dirname(path);

  if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0)
    return 0;

  mask = umask(0);
  umask(mask & ~0300);          /* ensure intermediate dirs are wx */

  p = path;
  while (1) {
    /* This loop tokenizing the pathname by replacing the first
     * non-'/' character into '\0'.  The replaced character is saved
     * in 'saved_char'. */
    saved_char = '\0';
    while (*p) {
      if (*p == '/') {
        while (*++p == '/')
          ;
        saved_char = *p;
        *p = '\0';
        break;
      }
      ++p;
    }

    if (!saved_char)
      umask(mask);

    if (mkdir(path, 0777) < 0) {
      if (errno != EEXIST) {
#ifdef TEST_WRITEPID
        fprintf(stderr, "mkdir: |%s| failed, errno = %d\n", path, errno);
#endif
        saved_errno = errno;
        goto end;
      }
      else {
#ifdef TEST_WRITEPID
        fprintf(stderr, "mkdir: |%s| exist\n", path);
#endif
      }
    }
    else {
#ifdef TEST_WRITEPID
      fprintf(stderr, "mkdir: |%s| success\n", path);
#endif
    }

    if (!saved_char)
      break;
    *p = saved_char;
  }
  ret = 0;

 end:
  free(path);
  umask(mask);
  errno = saved_errno;
  return ret;
}


int
copy_files(const char *

#ifdef TEST_FILEUTIL
int
test_filename_basename_imm(int argc, char *argv[])
{
  static const char *names[] = {
    "/home/cinsk/src/",
    "/usr/include/stdio.h",
    "/home/cinsk/.emacs",
    "/home/cinsk/.emacs.el",
    "/",
    "",
    NULL,
  };
  int i;

  for (i = 0; names[i] != NULL; i++) {
    printf("filename_basename_imm(\"%s\") == |%s|\n",
           names[i],
           filename_basename_imm(names[i]));
  }
  return 0;
}


int
test_filename_extension_imm(int argc, char *argv[])
{
  static const char *names[] = {
    "/home/cinsk/src/",
    "/usr/lib.inc/stdio.h",
    "/usr/lib.inc/.emacs",
    "/lib/hello.java",
    "/home/cinsk/.emacs.el",
    "/",
    "",
    NULL,
  };
  int i;

  for (i = 0; names[i] != NULL; i++) {
    printf("filename_extension_imm(\"%s\") == |%s|\n",
           names[i],
           filename_extension_imm(names[i]));
  }
  return 0;
}


int
test_filename_directory(int argc, char *argv[])
{
  char p1[] = "/home/cinsk/src/";
  char p2[] = "/usr/lib.inc/stdio.h";
  char p3[] = "/usr/lib.inc/.emacs";
  char p4[] = "hello.java";
  char p5[] = "/.emacs.el";
  char p6[] = "/";
  char p7[] = "";
  char *names[] = { p1, p2, p3, p4, p5, p6, p7, NULL };
  int i;

  for (i = 0; names[i] != NULL; i++) {
    printf("filename_directory(\"%s\") == ", names[i]);
    printf("|%s|\n", filename_directory(names[i]));
  }
  return 0;
}


int
test_filename_join(int argc, char *argv[])
{
  char *p;
  int i;
  char *args[] = {
    "/home/cinsk", "hello.c",
    "/home/", NULL,
    NULL, "hello.c",
    "cinsk", "src/world.sh",
    "hello", "//there",
  };

  if (argc == 0) {
    argc = sizeof(args) / sizeof(char *);
    argv = args;
  }
  for (i = 1; i < argc; i += 2) {
    p = filename_join(argv[i - 1], argv[i]);
    printf("%s + %s = |%s|\n", argv[i - 1], argv[i], p);
    free(p);
  }

  return 0;
}


int
main(int argc, char *argv[])
{
  test_filename_basename_imm(argc, argv);
  puts("\f");
  test_filename_extension_imm(argc, argv);
  puts("\f");
  test_filename_directory(argc, argv);
  puts("\f");
  test_filename_join(argc - 1, argv + 1);
  puts("\f");

  delete_directory(argv[1], 1);
  return 0;
}
#endif  /* TEST_FILEUTIL */
