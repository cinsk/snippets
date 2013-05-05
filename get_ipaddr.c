#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include "get_ipaddr.h"


char *
get_ipv4_address(int flags, int index)
{
  return get_ip_address(AF_INET, flags, index);
}


char *
get_ipv6_address(int flags, int index)
{
  return get_ip_address(AF_INET6, flags, index);
}


char *
get_ip_address(int family, int flags, int index)
{
  struct ifaddrs *addrs;
  struct ifaddrs *p;
  char host[NI_MAXHOST];
  int ret;
  int count;
  char *r = NULL;

  if (getifaddrs(&addrs) != 0)
    return NULL;

  for (count = 0, p = addrs; p != NULL; p = p->ifa_next) {
    int fam = p->ifa_addr->sa_family;

    // if (family != AF_INET && family != AF_INET6)
    if (fam != family)
      /* We're only interested on INET */
      continue;

    if (strcmp(p->ifa_name, "lo") == 0) {
      /* skip all interface that has name "lo" */
      continue;
    }

    if (index <= count) {
      ret = getnameinfo(p->ifa_addr,
                        (fam == AF_INET) ?
                        sizeof(struct sockaddr_in) :
                        sizeof(struct sockaddr_in6),
                        host, NI_MAXHOST,
                        NULL, 0, flags);
      if (ret == 0) {
        if (fam == AF_INET6) {
          /* remove zone index (e.g. "eth0" in
             "xxxx::xxxx::xxxx::xxxx::xxxx%eth0") */
          char *d = strrchr(host, '%');
          if (d)
            *d = '\0';
        }

        r = strdup(host);
      }
      else
        fprintf(stderr, "getnameinfo(3) failed: %s\n", gai_strerror(ret));
      break;
    }
    count++;
  }
  freeifaddrs(addrs);
  return r;
}


#ifdef TEST_GETIPADDR
int
main(void)
{
  char *address = get_ipv4_address(NI_NUMERICHOST, 0);
  char *address6 = get_ipv6_address(NI_NUMERICHOST, 0);

  printf("%s\n", address);

  free(address);


  printf("%s\n", address6);

  free(address6);
  return 0;
}
#endif  /* TEST_GETIPADDR */
