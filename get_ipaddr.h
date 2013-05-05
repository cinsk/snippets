#ifndef GETIPADDR_H__
#define GETIPADDR_H__

#include <netdb.h>

/* This indirect using of extern "C" { ... } makes Emacs happy */
#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */

BEGIN_C_DECLS

extern char *get_ip_address(int family, int flags, int index);
extern char *get_ipv4_address(int flags, int index);
extern char *get_ipv6_address(int flags, int index);

END_C_DECLS

#endif  /* GETIPADDR_H__ */
