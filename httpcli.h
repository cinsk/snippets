#ifndef HTTPCLI_H__
#define HTTPCLI_H__

struct http_header_pair {
  char *key;
  char *value;
};

/* All members of struct http_request are read-only.
 * If commented as 'reserved', you should not access that member. */
struct http_request {
  CURL *curl;
  struct curl_slist *headers;   /* reserved */
  struct curl_slist *persistant_hdrs; /* reserved */
  const char *url;

  CURLcode error;               /* Last curl_easy_perform() code */
  long status;                  /* Last HTTP Status */

  struct obstack resp_pool;     /* reserved */
  char *resp_pool_dummy;        /* reserved */

  struct http_header_pair *resp; /* ptr to the last response headers */
  size_t resp_size;              /* size of RESP */

  /* Note that RESP_ALL may contain empty (key == value == NULL)
   * elements, which are the delimiters for separating each
   * reqeust. */
  struct http_header_pair *resp_all; /* ptr to all response headers */
  size_t resp_all_size;              /* size of RESP_ALL */

  struct obstack body_pool;     /* reserved */
  char *body_pool_dummy;        /* reserved */

  /* Any member from now on is relying on STACK, and it will be set to NULL
   * by httpcli_clear_state(). */
  //char *resp_headers;
  char *body;
};

/*
 * Create new http_request struct.
 *
 * Should be freed via httpcli_delete().
 */
struct http_request *httpcli_new(void);

/*
 * Delete the http_request struct.
 */
void httpcli_delete(struct http_request *req);

/*
 * Clear HTTP headers for the next request.
 *
 * Note that this only deletes headers that are registed by
 * httpcli_add_header().
 */
struct http_request *httpcli_clear_headers(struct http_request *req);

/*
 * Add a custom HTTP header for the next request.
 *
 * Note that all added headers by this function will be used by the
 * next request, then the headers will be cleared.
 */
int httpcli_add_header(struct http_request *req, const char *format, ...)
  __attribute__ ((format (printf, 2, 3)));

/*
 * Add a custom HTTP header for all requests.
 *
 * Unlike httpcli_add_header(), headers which are added by this
 * function will be used during the lifetime of http_request struct.
 */
int httpcli_add_persistant_header(struct http_request *req,
                                  const char *format, ...)
  __attribute__ ((format (printf, 2, 3)));

/*
 * Perform HTTP GET request to the given URL.
 *
 * On success, the BUF will be set to the contents of the response,
 * which will be automatically deleted at the next http request.
 *
 * If SIZE is non-NULL, it will be set to the size of the contents.
 *
 * For your own convienice, the content should be always
 * null-terminated (by adding extra '\0' byte.)  However, the SIZE
 * will keep the original size (by not counting the extra byte.)
 *
 * It will return zero on success, otherwise will return -1.
 * The details (the return value of curl_easy_perform) will be stored in
 * req->error.
 *
 * HTTP status will be stored in req->status.
 */
int httpcli_get_buffer(struct http_request *req, const char *url,
                       char **buf, size_t *size);

/*
 * Perform HTTP GET request to the given URL.
 *
 * The contents will be saved in the FILE stream, FP.
 * Note that this function will close FP.
 */
int httpcli_get_stream(struct http_request *req, const char *url, FILE *fp);

/*
 * Perform HTTP PUT request to the given URL.
 *
 * The content is from PATHNAME.  If MIME_TYPE is NULL, it will be
 * automatically detected.  If MIME_TYPE is (char *)-1, the
 * Content-Type header will not be sent.
 *
 * The response body is stored in REQ->body, which is guaranteed to be
 * null-terminated-string. (And it will be released at the next request.)
 */
int httpcli_put_file(struct http_request *req, const char *url,
                     const char *pathname, const char *mime_type);

/*
 * Perform HTTP PUT reqeust to the given URL.
 *
 * The content is from FP.
 *
 * CONTENT_TYPE is the MIME type of the contents.  If it is NULL, the
 * type is automatically detected.  if it is (char *)-1, no
 * Content-Type header will be generated.
 */
int httpcli_put_stream(struct http_request *req, const char *url, FILE *fp,
                       const char *content_type);

#endif  /* HTTPCLI_H__ */
