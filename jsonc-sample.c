#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <json/json.h>


struct json_object *json_object_object_xget(struct json_object *obj,
                                            /* keys */ ...);
struct json_object *json_object_object_vget(struct json_object *obj,
                                            va_list ap);
struct json_object *json_object_object_getv(struct json_object *obj,
                                            const char *keys[],
                                            size_t nkeys);

int
main(int argc, const char *argv[])
{
  struct json_object *json;
  struct json_object *obj;
  if (argc < 2) {
    fprintf(stderr, "usage: %s JSON-FILE KEY...\n", argv[0]);
    return 1;
  }

  json = json_object_from_file(argv[1]);
  if (!json) {
    fprintf(stderr, "error: failed to load JSON document\n");
    return 1;
  }

  obj = json_object_object_getv(json, argv + 2, argc - 2);

  printf("%s\n", json_object_to_json_string(obj));
  json_object_put(json);
  return 0;
}


struct json_object *
json_object_object_xget(struct json_object *obj, /* keys */ ...)
{
  va_list ap;

  va_start(ap, obj);
  obj = json_object_object_vget(obj, ap);
  va_end(ap);

  return obj;
}


struct json_object *
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

struct json_object *
json_object_object_getv(struct json_object *obj, const char *keys[],
                        size_t nkeys)
{
  int i;

  for (i = 0; keys[i] != NULL && i < nkeys; i++) {
    if (json_object_get_type(obj) != json_type_object)
      return NULL;
    obj = json_object_object_get(obj, keys[i]);
    if (!obj)
      return NULL;
  }
  return obj;
}


