#ifndef MESSAGE_H__
#define MESSAGE_H__

/*
 * Efficient log message routine.
 *
 * - thread safety for all producing function/macros.
 * - easy configuration from external configuration file.
 * - message log into a file/syslog/multiplexer.
 * - message filtering by the priority level.
 *
 * Currently, thread-safey of the producing message is relying on
 * the write(2) call with less than PIPE_BUF data.
 *
 * If the absolute robustness is required, I can make the output
 * routine completely critical region so that I can accept a lengthy
 * log (greater than PIPE_BUF).
 */
#ifndef FALSE
#define FALSE   0
#define TRUE    (!FALSE)
#endif

#include <stddef.h>
#include <sys/types.h>
#include <signal.h>

struct msg_category;

#define MCT_NONE        0
#define MCT_FILE        1
#define MCT_MULTIPLEX   2
#define MCT_SYSLOG      3

/*
 * You cannot change the value of the following MCL_ macroes
 * unless you modified syslog_priority() accordingly.
 */
#define MCL_DEBUG       500
#define MCL_INFO        400
#define MCL_WARN        300
#define MCL_ERR         200
#define MCL_FATAL       100

extern struct msg_category *MC_DEFAULT;

#define MCO_NONE        0x000
#define MCO_SIGHUP      0x001
#define MCO_NONBLOCK    0x002

extern void message_init(const char *pathname, ...);

extern void message_thread_init(const char *thread_name);

extern void message_signal_handler(int signo, siginfo_t *info, void *uap);

extern struct msg_category *message_get_category(const char *name);
extern void message_unget_category(struct msg_category *category);

extern void message_c(struct msg_category *category,
                      int level, const char *msg, ...)
  __attribute__((format(printf, 3, 4)));

#define message(...)    message_c(MC_DEFAULT, MCL_WARN, __VA_ARGS__)

#define m_debug(...)    message_c(MC_DEFAULT, MCL_DEBUG, __VA_ARGS__)
#define m_info(...)     message_c(MC_DEFAULT, MCL_INFO,  __VA_ARGS__)
#define m_warn(...)     message_c(MC_DEFAULT, MCL_WARN,  __VA_ARGS__)
#define m_err(...)      message_c(MC_DEFAULT, MCL_ERR,   __VA_ARGS__)
#define m_fatal(...)    message_c(MC_DEFAULT, MCL_FATAL, __VA_ARGS__)

#define c_debug(c, ...)    message_c((c), MCL_DEBUG, __VA_ARGS__)
#define c_info(c, ...)     message_c((c), MCL_INFO,  __VA_ARGS__)
#define c_warn(c, ...)     message_c((c), MCL_WARN,  __VA_ARGS__)
#define c_err(c, ...)      message_c((c), MCL_ERR,   __VA_ARGS__)
#define c_fatal(c, ...)    message_c((c), MCL_FATAL, __VA_ARGS__)

extern struct msg_category *message_file_category(const char *name,
                                                      const char *pathname);
extern struct msg_category *message_multiplex_category(const char *name,
                                                           ...);
extern int message_multiplexer_add(struct msg_category *multiplexer,
                                   struct msg_category *target);

extern int message_set_level(struct msg_category *category, int level);


#endif  /* MESSAGE_H__ */
