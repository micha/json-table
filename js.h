/*
 * JSON parser
 *****************************************************************************/

#ifndef JS_H
#define JS_H

#include <stddef.h>
#include "buffer.h"
#include "util.h"

typedef enum {
  JS_NONE,
  JS_OBJECT,
  JS_ARRAY,
  JS_NULL,
  JS_NUMBER,
  JS_TRUE,
  JS_FALSE,
  JS_STRING,
  JS_PAIR,
  JS_ITEM
} jstype_t;

typedef enum {
  JS_OKAY,
  JS_EBUG,
  JS_EPARSE,
  JS_EDONE
} jserr_t;

typedef struct {
  jstype_t type;
  size_t start;
  size_t end;
  size_t idx;
  size_t parent;
  size_t first_child;
  size_t next_sibling;
  size_t parsed;
} jstok_t;

typedef struct {
  size_t bufpos;
  size_t parserpos;
} jsstate_t;

typedef struct {
  FILE *in;
  Buffer *js;
  size_t pos;
  jstok_t *toks;
  size_t curtok;
  size_t toks_size;
  size_t depth;
} jsparser_t;

jstok_t *js_tok(jsparser_t *p, size_t t);

// predicates

int js_is_string(jstok_t *tok);

int js_is_array(jstok_t *tok);

int js_is_object(jstok_t *tok);

int js_is_collection(jstok_t *tok);

int js_is_empty(jstok_t *tok);

int js_is_pair(jstok_t *tok);

int js_is_item(jstok_t *tok);

// parser

void js_alloc(jsparser_t **p, FILE *in, size_t toks_size);

void js_free(jsparser_t **p);

jserr_t js_parse(jsparser_t *p, size_t t);

jserr_t js_parse_one(jsparser_t *p, size_t *t);

void js_reset(jsparser_t *p);

void js_save(jsparser_t *p, jsstate_t *s);

void js_restore(jsparser_t *p, jsstate_t *s);

// accessors

size_t js_obj_get(jsparser_t *p, size_t obj, const char *key);

size_t js_obj_get_fuzzy(jsparser_t *p, size_t obj, const char *key);

size_t js_array_get(jsparser_t *p, size_t ary, size_t idx);

// create new nodes

size_t js_create_index(jsparser_t *p, size_t idx);

// printing, unescaping

jserr_t js_print(jsparser_t *p, size_t t, Buffer *b, int json);

jserr_t js_print_info(jsparser_t *p, size_t t, Buffer *buf);

char *js_unescape_string(char *in);

#endif
