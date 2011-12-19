#define _GNU_SOURCE

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <curl/curl.h>
#include <obstack.h>

#include "httpcli.h"

#define obstack_chunk_alloc     malloc
#define obstack_chunk_free      free

#define HDR_LEN_MAX     1024

#define MIME_DELIMS     " \t\v\r\n"
#define HTTP_HDR_DELIMS " \t\v\r\n"

struct ext_mime {
  char *postfix;
  char *mime;
};

struct obstack mime_pool;
struct ext_mime *mime_table;
size_t mime_table_entries;


static int mime_ent_compare(const void *lhs, const void *rhs);
const char *mime_table_lookup(const char *url_like);
int mime_table_init(const char *pathname);
void mime_table_free(void);

static int parse_http_header(struct http_header_pair *pair,
                             char *line, size_t size);
static __inline__ void httpcli_clear_response_hdrs(struct http_request *req);
static __inline__ void httpcli_clear_state(struct http_request *req);

static size_t receive_buffer_func(char *ptr, size_t size,
                                  size_t nmemb, void *userdata);
static size_t receive_stream_func(char *ptr, size_t size, size_t nmemb,
                                  void *userdata);
static void httpcli_unset_headers(struct http_request *req);
static void httpcli_set_headers(struct http_request *req);
static int httpcli_perform(struct http_request *req, const char *url);


static int
mime_ent_compare(const void *lhs, const void *rhs)
{
  struct ext_mime *l = (struct ext_mime *)lhs;
  struct ext_mime *r = (struct ext_mime *)rhs;

  return strcmp(l->postfix, r->postfix);
}


const char *
mime_table_lookup(const char *url_like)
{
  char ext[32] = { 0, };
  struct ext_mime key, *found;
  const char *p, *end;
  int len;

  len = strlen(url_like);

  end = url_like + len;

  for (p = end; p >= url_like; p--)
    if (*p == '?')
      end = p;

  for (p = end; p >= url_like; p--) {
    if (*p == '.') {
      if (end - p < 32 - 1)
        memcpy(ext, p + 1, end - p);
      break;
    }
  }

  if (ext[0] == 0)
    return NULL;

  key.postfix = ext;
  key.mime = NULL;
  found = (struct ext_mime *)bsearch(&key, mime_table,
                                     mime_table_entries, sizeof(key),
                                     mime_ent_compare);
  if (found)
    return found->mime;
  else
    return NULL;
}


void
mime_table_free(void)
{
  int i;
  for (i = 0; i < mime_table_entries; i++) {
    free(mime_table[i].postfix);
    free(mime_table[i].mime);
  }

  obstack_free(&mime_pool, NULL);
  mime_table = NULL;
  mime_table_entries = 0;
}


int
mime_table_init(const char *pathname)
{
  FILE *fp;
  char line[LINE_MAX];
  char *type, *ext, *saveptr = NULL;
  struct ext_mime ent = { 0, 0};

  obstack_init(&mime_pool);

  if (!pathname)
    pathname = "/etc/mime.types";

  fp = fopen(pathname, "r");
  while (fgets(line, LINE_MAX, fp) != NULL) {
    if (line[0] == '\n' || line[0] == '#')
      continue;

    type = strtok_r(line, MIME_DELIMS, &saveptr);
    if (!type)
      continue;

    while ((ext = strtok_r(NULL, MIME_DELIMS, &saveptr)) != NULL) {
      ent.postfix = strdup(ext);
      ent.mime = strdup(type);
      obstack_grow(&mime_pool, &ent, sizeof(ent));
    }
  }
  fclose(fp);

  mime_table_entries = obstack_object_size(&mime_pool) / sizeof(ent);
  mime_table = obstack_finish(&mime_pool);

  qsort(mime_table, mime_table_entries, sizeof(struct ext_mime),
        mime_ent_compare);

  return 0;
}


/*
 * According to libcurl, we cannot assume that LINE is null-terminated.
 *
 * Returns 1 if it parsed the header successfully, 0 if the header is
 * empty, -1 on error.
 */
static int
parse_http_header(struct http_header_pair *pair, char *line, size_t size)
{
  char *p;
  size_t sz;

  if (size == 0)
    return -1;

  p = line + strspn(line, HTTP_HDR_DELIMS);
  if (!p || *p == '\0') {
    pair->key = pair->value = 0;
    return 0;
  }

  sz = size - (p - line) + 1;
  if ((pair->key = malloc(sz)) == NULL)
    return -1;

  memcpy(pair->key, p, sz - 1);
  pair->key[sz - 1] = '\0';

  for (p = pair->key + sz - 2; p >= pair->key; p--) {
    if (*p == '\r' || *p == '\n')
      *p = '\0';
    else
      break;
  }


  p = strchr(pair->key, ':');
  if (!p)
    pair->value = 0;
  else {
    *p = '\0';
    pair->value = p + 1;
    pair->value += strspn(pair->value, HTTP_HDR_DELIMS);
  }
  return 1;
}


static __inline__ void
httpcli_clear_response_hdrs(struct http_request *req)
{
  int i;

  if (!req->resp_all)
    return;

  for (i = 0; i < req->resp_all_size; i++) {
    if (req->resp_all[i].key) {
      free(req->resp_all[i].key);
      req->resp_all[i].key = 0;
      req->resp_all[i].value = 0;
    }
  }

  if (req->resp_pool_dummy)
    obstack_free(&req->resp_pool, req->resp_pool_dummy);
  req->resp_pool_dummy = obstack_alloc(&req->resp_pool, 1);

  req->resp_all = 0;
  req->resp_size = 0;
  req->resp = 0;
  req->resp_size = 0;
}


/*
 * Clear all possible state(s) that should be reset between
 * curl_easy_perform().
 *
 * All HTTP Request (GET/POST/PUT/...) should be cleared,
 * and all members of struct http_request that depend on obstack.
 */
static __inline__ void
httpcli_clear_state(struct http_request *req)
{
  curl_easy_setopt(req->curl, CURLOPT_UPLOAD, 0);
  curl_easy_setopt(req->curl, CURLOPT_PUT, 0);
  curl_easy_setopt(req->curl, CURLOPT_POST, 0);
  curl_easy_setopt(req->curl, CURLOPT_NOBODY, 0);

  curl_easy_setopt(req->curl, CURLOPT_WRITEFUNCTION, NULL);
  curl_easy_setopt(req->curl, CURLOPT_WRITEDATA, NULL);
  curl_easy_setopt(req->curl, CURLOPT_READFUNCTION, NULL);
  curl_easy_setopt(req->curl, CURLOPT_READDATA, NULL);

  httpcli_clear_response_hdrs(req);

  if (req->body_pool_dummy)
    obstack_free(&req->body_pool, req->body_pool_dummy);
  req->body = NULL;
  req->body_pool_dummy = obstack_alloc(&req->body_pool, 1);
}


struct http_request *
httpcli_new(void)
{
  struct http_request *p;
  p = malloc(sizeof(*p));
  p->curl = curl_easy_init();
  p->headers = NULL;
  p->persistant_hdrs = NULL;
  p->url = NULL;

  obstack_init(&p->resp_pool);
  p->resp_pool_dummy = obstack_alloc(&p->resp_pool, 1);
  p->resp = p->resp_all = NULL;
  p->resp_size = p->resp_all_size = 0;

  obstack_init(&p->body_pool);
  p->body_pool_dummy = obstack_alloc(&p->body_pool, 1);
  return p;
}


void
httpcli_delete(struct http_request *req)
{
  curl_easy_cleanup(req->curl);
  curl_slist_free_all(req->headers);
  curl_slist_free_all(req->persistant_hdrs);

  if (req->url)
    free((void *)req->url);

  httpcli_clear_response_hdrs(req);

  obstack_free(&req->resp_pool, NULL);
  obstack_free(&req->body_pool, NULL);

#ifndef NDEBUG
  req->curl = NULL;
  req->headers = req->persistant_hdrs = NULL;
  req->url = NULL;
  req->body_pool_dummy = NULL;
  req->body = NULL;
#endif  /* NDEBUG */

  free(req);
}


struct http_request *
httpcli_clear_headers(struct http_request *req)
{
  curl_easy_setopt(req->curl, CURLOPT_HTTPHEADER, NULL);

  if (req->headers) {
    curl_slist_free_all(req->headers);
    req->headers = 0;
  }
  return req;
}


static size_t
receive_header_func(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  struct http_request *req = (struct http_request *)userdata;
  size_t total = size * nmemb;
  struct http_header_pair pair;
  int ret;

  if (total > 0) {
    ret = parse_http_header(&pair, ptr, total);
    if (ret >= 0) {
      obstack_grow(&req->resp_pool, &pair, sizeof(pair));
      fprintf(stderr, "  receive_header(%d): key(%s) value(%s)\n", total,
              pair.key, pair.value);
    }
#if 0
    else if (ret == 0) {
      /* We do not know if this means whether the final HTTP header is
       * parsed, or it is the beginning of the another header set
       * (occurs if redirection is applied.) */
      fprintf(stderr, "  receive_header(%d): EMPTY\n", total);
    }
#endif  /* 0 */
    else {
      /* TODO: how can handle this error? */
    }
  }

  return size * nmemb;
}


static size_t
receive_buffer_func(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  // todo: write size * nmemb bytes from ptr to your storage
  struct http_request *req = (struct http_request *)userdata;
  size_t total = size * nmemb;

  fprintf(stderr, "  receive_buffer_func(%p, %zu, %zu, %p)\n",
          ptr, size, nmemb, userdata);
  if (total > 0)
    obstack_grow(&req->body_pool, ptr, total);

  return total;
}


static size_t
receive_stream_func(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  // todo: write size * nmemb bytes from ptr to your storage
  struct http_request *req = (struct http_request *)userdata;
  size_t total = size * nmemb;

  if (total > 0)
    obstack_grow(&req->body_pool, ptr, total);

  return total;
}


/*
 * Split the headers list, which is merged by httpcli_set_headers().
 */
static void
httpcli_unset_headers(struct http_request *req)
{
  struct curl_slist *p, *q;

  if (req->headers && req->persistant_hdrs) {
      for (q = NULL, p = req->persistant_hdrs;
           p != req->headers && p != 0; q = p, p = p->next)
        ;
      q->next = NULL;
  }

  httpcli_clear_headers(req);
}


/*
 * struct http_request maintains two list of HTTP headers.
 *
 * Its member, 'headers' will contain the headers that are only used
 * for the next curl_easy_perform(), whereas 'persistant_hdrs' will
 * contain the headers that are used for all requests in this struct
 * http_request.
 *
 * Thus, this function will be used before performing HTTP request, to
 * merge both headers, and to set the headers via CURLOPT_HTTPHEADER.
 *
 * Note that after merging, 'persistant_hdrs' will points the merged
 * header list.
 */
static void
httpcli_set_headers(struct http_request *req)
{
  struct curl_slist *p, *q;

  if (req->headers) {
    if (req->persistant_hdrs) {
      for (q = NULL, p = req->persistant_hdrs; p != NULL; q = p, p = p->next)
        ;
      /* Q points the last curl_slist node. */
      q->next = req->headers;

      curl_easy_setopt(req->curl, CURLOPT_HTTPHEADER, req->persistant_hdrs);
    }
    else
      curl_easy_setopt(req->curl, CURLOPT_HTTPHEADER, req->headers);
  }
  else {
    if (req->persistant_hdrs)
      curl_easy_setopt(req->curl, CURLOPT_HTTPHEADER, req->persistant_hdrs);
    else
      curl_easy_setopt(req->curl, CURLOPT_HTTPHEADER, NULL);
  }
}


int
httpcli_add_persistant_header(struct http_request *req,
                              const char *format, ...)
{
  va_list ap;
  char *ptr;
  int ret;

  va_start(ap, format);
  ret = vasprintf(&ptr, format, ap);
  va_end(ap);

  if (ret > 0) {
    req->persistant_hdrs = curl_slist_append(req->persistant_hdrs, ptr);
    free(ptr);
    return 0;
  }
  else
    return -1;
}


int
httpcli_add_header(struct http_request *req, const char *format, ...)
{
  va_list ap;
  char *ptr;
  int ret;

  va_start(ap, format);
  ret = vasprintf(&ptr, format, ap);
  va_end(ap);

  if (ret > 0) {
    req->headers = curl_slist_append(req->headers, ptr);
    free(ptr);
    return 0;
  }
  else
    return -1;
}


static int
httpcli_perform(struct http_request *req, const char *url)
{
  if (url) {
    if (req->url)
      free((void *)req->url);
    req->url = strdup(url);
    if (!req->url)
      return -1;
  }

  curl_easy_setopt(req->curl, CURLOPT_URL, req->url);

  httpcli_set_headers(req);
  req->error = curl_easy_perform(req->curl);
  httpcli_unset_headers(req);

  {
    /* TODO: Finalize the response headers */
    size_t hdsize;
    struct http_header_pair *p;
    int i;

    hdsize = obstack_object_size(&req->resp_pool) / sizeof(*p);
    p = obstack_finish(&req->resp_pool);

    req->resp_all_size = hdsize;
    req->resp_all = p;

    if (hdsize > 0) {
      /* The index [hdsize - 1] should point the empty element. */
      for (i = hdsize - 2; i >= 0; i--) {
        if (p[i].key == 0)
          break;
      }
      req->resp = p + i + 1;
      req->resp_size = hdsize - i - 2;
    }
  }

  if (req->error == CURLE_OK)
    curl_easy_getinfo(req->curl, CURLINFO_RESPONSE_CODE, &req->status);
  else
    req->status = -1;

  return 0;
}

int
httpcli_get_buffer(struct http_request *req, const char *url,
                   char **buf, size_t *size)
{
  assert(buf != NULL);

  httpcli_clear_state(req);

  curl_easy_setopt(req->curl, CURLOPT_FOLLOWLOCATION, 1);

  curl_easy_setopt(req->curl, CURLOPT_HEADERFUNCTION, receive_header_func);
  curl_easy_setopt(req->curl, CURLOPT_HEADERDATA, req);

  curl_easy_setopt(req->curl, CURLOPT_WRITEFUNCTION,
                   receive_buffer_func);
  curl_easy_setopt(req->curl, CURLOPT_WRITEDATA, req);

  if (httpcli_perform(req, url) < 0)
    return -1;

  if (size)
    *size = obstack_object_size(&req->body_pool);
  obstack_1grow(&req->body_pool, '\0');

  req->body = *buf = obstack_finish(&req->body_pool);

  return -(int)req->error;
}


int
httpcli_get_stream(struct http_request *req, const char *url, FILE *fp)
{
  assert(fp != NULL);

  httpcli_clear_state(req);

  curl_easy_setopt(req->curl, CURLOPT_HEADERFUNCTION, receive_header_func);
  curl_easy_setopt(req->curl, CURLOPT_HEADERDATA, req);

  curl_easy_setopt(req->curl, CURLOPT_WRITEFUNCTION, NULL);
  curl_easy_setopt(req->curl, CURLOPT_WRITEDATA, fp);

  if (httpcli_perform(req, url) < 0)
    return -1;

  fclose(fp);

  return -(int)req->error;
}


int
httpcli_put_file(struct http_request *req, const char *url,
                 const char *pathname, const char *mime_type)
{
  FILE *fp;
  int ret;

  fp = fopen(pathname, "r");
  if (!fp)
    return -1;

  if (!mime_type)
    mime_type = mime_table_lookup(pathname);

  ret = httpcli_put_stream(req, url, fp, mime_type);
  fclose(fp);

  return ret;
}


int
httpcli_put_stream(struct http_request *req, const char *url, FILE *fp,
                   const char *content_type)
{
  struct stat statbuf;

  assert(fp != NULL);

  httpcli_clear_state(req);

  if (fstat(fileno(fp), &statbuf) < 0)
    return -1;
  curl_easy_setopt(req->curl, CURLOPT_INFILESIZE_LARGE,
                   (curl_off_t)statbuf.st_size);

  if (content_type && content_type != (const char *)-1)
    httpcli_add_header(req, "Content-Type: %s", content_type);

  curl_easy_setopt(req->curl, CURLOPT_HTTPHEADER, req->headers);

  curl_easy_setopt(req->curl, CURLOPT_HEADERFUNCTION, receive_header_func);
  curl_easy_setopt(req->curl, CURLOPT_HEADERDATA, req);

  curl_easy_setopt(req->curl, CURLOPT_READFUNCTION, NULL);
  curl_easy_setopt(req->curl, CURLOPT_READDATA, fp);

  curl_easy_setopt(req->curl, CURLOPT_WRITEFUNCTION,
                   receive_buffer_func);
  curl_easy_setopt(req->curl, CURLOPT_WRITEDATA, req);

  curl_easy_setopt(req->curl, CURLOPT_UPLOAD, 1);

  if (httpcli_perform(req, url) < 0)
    return -1;

  obstack_1grow(&req->body_pool, '\0');
  req->body = obstack_finish(&req->body_pool);

  return -(int)req->error;
}


int
main(int argc, char *argv[])
{
  mime_table_init(NULL);

  curl_global_init(CURL_GLOBAL_ALL);

#if 0
  {
    int i;
    for (i = 0; i < mime_table_entries; i++) {
      printf("%20s - %s\n", mime_table[i].postfix, mime_table[i].mime);
    }
  }
#endif  /* 0 */

  {
    const char *mime;
    mime = mime_table_lookup(argv[1]);
    printf("%s\n", mime ? mime : "*NIL*");
  }

  if (1) {
    struct http_request *req;
    char *buf = NULL;
    size_t size = 0;
    int ret;

    req = httpcli_new();

    ret = httpcli_get_buffer(req, "http://www.cinsk.org/", &buf, &size);
    printf("httpcli_put_file returns %d\n", ret);
    printf("status: %ld\n", req->status);
    // printf("body: %s\n", req->body);
    {
      int i;
      for (i = 0; i < req->resp_size; i++)
        printf("    %s = %s\n", req->resp[i].key, req->resp[i].value);
    }

    ret = httpcli_get_buffer(req, argv[1], &buf, &size);
    printf("httpcli_put_file returns %d\n", ret);
    printf("status: %ld\n", req->status);
    //printf("body: %s\n", req->body);
    {
      int i;
      for (i = 0; i < req->resp_size; i++)
        printf("    %s = %s\n", req->resp[i].key, req->resp[i].value);
    }

    httpcli_delete(req);

  }
  else {
    struct http_request *req;
    char *url;
    int ret;

    req = httpcli_new();

    asprintf(&url,
             "http://localhost:15984/rootfs/2054-164183/animdata?rev=%s",
             argv[1]);
    ret = httpcli_put_file(req, url,
                           "/home/cinsk/ultima/ultima6/animdata",
                           "image/jpeg");
    printf("httpcli_put_file returns %d\n", ret);
    printf("status: %ld\n", req->status);
    //printf("body: %s\n", req->body);
    {
      int i;
      for (i = 0; i < req->resp_size; i++)
        printf("    %s = %s\n", req->resp[i].key, req->resp[i].value);
    }

    free(url);

    asprintf(&url,
             "http://localhost:15984/rootfs/2054-164184/animmask.vga?rev=%s",
             argv[2]);
    ret = httpcli_put_file(req, url,
                           "/home/cinsk/ultima/ultima6/animmask.vga",
                           "image/gif");
    printf("httpcli_put_file returns %d\n", ret);
    printf("status: %ld\n", req->status);
    //printf("body: %s\n", req->body);
    {
      int i;
      for (i = 0; i < req->resp_size; i++)
        printf("    %s = %s\n", req->resp[i].key, req->resp[i].value);
    }
    free(url);

    httpcli_delete(req);
  }

  mime_table_free();

  curl_global_cleanup();
  return 0;
}
