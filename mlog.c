#include <stdarg.h>
#include <time.h>

#include <syslog.h>

#include <mlog.h>


#define TIME_BUF_MAX    32

static FILE *mlog_stream = 0;
static int syslog_opened = 0;


#ifndef HAVE_VSYSLOG
static char mlog_buffer[MLOG_BUFFER_MAX];
#endif


static int
using_stream(void)
{
  if (mlog_stream)
    return 1;

  if (!syslog_opened) {
    openlog(program_name, LOG_CONS | LOG_NOWAIT | LOG_PID, LOG_USER);
    atexit(closelog);
    syslog_opened = 1;
  }
  return 0;
}


int
mlog_set_stream(FILE *fp)
{
  fflush(fp);
  if (setvbuf(fp, 0, _IONBF, 0) != 0) {
    /* Warning: cannot empty the stream buffer. */
    return -1;
  }

  mlog_stream = fp;
  return 0;
}


FILE *
mlog_get_stream(void)
{
  return mlog_stream;
}


static const char *
current_time()
{
  struct tm *tmptr;
  time_t t;
  static char buf[TIME_BUF_MAX];
  int ret;

  t = time(0);
  tmptr = localtime(&t);
  ret = strftime(buf, TIME_BUF_MAX, "%b %d %H:%M:%S", tmptr);
  if (ret == TIME_BUF_MAX || ret == 0)
    return 0;
  return buf;
}


void
mlog(const char *format, ...)
{
  va_list ap;
  int ret;

  if (using_stream()) {
    fprintf(mlog_stream, "%s: %s[%d]: ",
            current_time(), program_name, (int)getpid());
    va_start(ap, format);
    vfprintf(mlog_stream, format, ap);
    va_end(ap);
    fputc('\n', mlog_stream);
  }
  else {
#ifdef HAVE_VSYSLOG
    vsyslog(LOG_INFO | LOG_USER, format, ap);
#else
    va_start(ap, format);
    ret = vsnprintf(mlog_buffer, MLOG_BUFFER_MAX, format, ap);
    va_end(ap);

    if (ret >= MLOG_BUFFER_MAX || ret < 0)
      mlog_buffer[MLOG_BUFFER_MAX - 1] = '\0';
    syslog(LOG_INFO | LOG_USER, "%s", mlog_buffer);
#endif /* HAVE_VSYSLOG */
  }
}


#ifdef MLOG_TEST
const char *program_name = "mlog";

int
main(int argc, char *argv[])
{
  FILE *fp;
  char buf[10];

  fp = fopen(argv[1], "a");
  if (!fp)
    return -1;
  mlog_set_stream(fp);

  MLOG(1, "hello, %s", "world");
  gets(buf);

  MLOG(1, "hello, %s", "world");
  gets(buf);

  MLOG(1, "hello, %s", "world");
  gets(buf);

  fclose(fp);
  return 0;
}
#endif /* MLOG_TEST */
