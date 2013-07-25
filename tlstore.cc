#include "tlstore.hh"
#include <string.h>
#include <stdlib.h>


class Locals {
  static tlstore<std::string> KEY_SESSION_ID;
  static tlstore<std::string> KEY_UTTERANCE_ID;
  static tlstore<std::string> KEY_DEVICE_ID;

public:
  static void set_sid(const std::string &s) {
    KEY_SESSION_ID.reset(new std::string(s));
  }
  static void set_uid(const std::string &s) {
    KEY_UTTERANCE_ID.reset(new std::string(s));
  }
  static void set_did(const std::string &s) {
    KEY_DEVICE_ID.reset(new std::string(s));
  }

  static const std::string sid() {
    std::string *p = KEY_SESSION_ID.get();
    if (p)
      return *p;
    else
      return std::string();
  }
  static const std::string uid() {
    std::string *p = KEY_UTTERANCE_ID.get();
    if (p)
      return *p;
    else
      return std::string();
  }
  static const std::string did() {
    std::string *p = KEY_DEVICE_ID.get();
    if (p)
      return *p;
    else
      return std::string();
  }
};

#define TLOCAL_SID()    (Locals::sid())
#define TLOCAL_UID()    (Locals::uid())
#define TLOCAL_DID()    (Locals::did())

#define TLOCAL_SETSID(id)       (Locals::set_sid(id))
#define TLOCAL_SETUID(id)       (Locals::set_uid(id))
#define TLOCAL_SETDID(id)       (Locals::set_did(id))


tlstore<std::string> Locals::KEY_SESSION_ID;
tlstore<std::string> Locals::KEY_UTTERANCE_ID;
tlstore<std::string> Locals::KEY_DEVICE_ID;


#ifdef _TEST
#include <stdio.h>

pthread_t child_threads[10];
const int nchildren = sizeof(child_threads) / sizeof(pthread_t);
int child_id = 0;


static void *
child_main(void *arg)
{
  int id = __sync_add_and_fetch(&child_id, 1);

  TLOCAL_SETSID("hello");
  TLOCAL_SETUID("world");
  TLOCAL_SETDID("there");

  printf("child%d: sid=%s, uid=%s, did=%s\n", id,
         TLOCAL_SID().c_str(), TLOCAL_UID().c_str(), TLOCAL_DID().c_str());

  TLOCAL_SETSID("hello");
  TLOCAL_SETUID("world");
  TLOCAL_SETDID("there");

  printf("child%d: sid=%s, uid=%s, did=%s\n", id,
         TLOCAL_SID().c_str(), TLOCAL_UID().c_str(), TLOCAL_DID().c_str());

  return 0;
}


int
main(void)
{
  for (int i = 0; i < nchildren; i++)
    pthread_create(child_threads + i, 0, child_main, 0);

  TLOCAL_SETSID("greeting");
  TLOCAL_SETUID("farewell");
  TLOCAL_SETDID("thee");

  printf("main: sid=%s, uid=%s, did=%s\n",
         TLOCAL_SID().c_str(), TLOCAL_UID().c_str(), TLOCAL_DID().c_str());

#if 0
  TLOCAL_SETSID("greeting");
  TLOCAL_SETUID("farewell");
  TLOCAL_SETDID("thee");

  printf("main: sid=%s, uid=%s, did=%s\n",
         TLOCAL_SID().c_str(), TLOCAL_UID().c_str(), TLOCAL_DID().c_str());
#endif  // 0

  for (int i = 0; i < nchildren; i++)
    pthread_join(child_threads[i], 0);
  return 0;
}

#endif  // _TEST
