/*
 * JSON parser
 *****************************************************************************/

#include "js.h"

/*
 * parser helpers
 *****************************************************************************/

inline jstok_t *js_tok(jsparser_t *p, size_t t) {
  return p->toks + t;
}

static inline const char *js(jsparser_t *p) {
  return (p->js)->buf + p->pos;
}

static void init_tok(jstok_t *tok) {
  tok->type = JS_NONE;
  tok->start = 0;
  tok->end = 0;
  tok->idx = 0;
  tok->parent = 0;
  tok->first_child = 0;
  tok->next_sibling = 0;
}

static size_t js_next_tok(jsparser_t *p) {
  if (p->curtok + 1 >= p->toks_size)
    p->toks = jrealloc(p->toks, sizeof(jstok_t) * (p->toks_size *= 2));
  init_tok(p->toks + (p->curtok += 1));
  return p->curtok;
}

static inline void js_ensure_buf(jsparser_t *p, size_t n) {
  while (p->pos + n + 1 >= (p->js)->pos)
    if (! buf_append_read(p->js, p->in)) break;
}

static size_t js_skip_ws(jsparser_t *p) {
  size_t start = p->pos;

  while (1) {
    js_ensure_buf(p, 1);
    if (! is_ws_char(js(p)[0])) break;
    p->pos++;
  }

  return p->pos - start;
}

static size_t js_scan_digits(jsparser_t *p) {
  size_t start = p->pos;

  while (1) {
    js_ensure_buf(p, 1);
    if (!is_digit_char(js(p)[0])) break;
    p->pos++;
  }

  return p->pos - start;
}

/*
 * JSON type predicates
 *****************************************************************************/

inline int js_is_array(jstok_t *tok) {
  return tok->type == JS_ARRAY;
}

inline int js_is_object(jstok_t *tok) {
  return tok->type == JS_OBJECT;
}

inline int js_is_pair(jstok_t *tok) {
  return tok->type == JS_PAIR;
}

inline int js_is_item(jstok_t *tok) {
  return tok->type == JS_ITEM;
}

inline int js_is_collection(jstok_t *tok) {
  return js_is_array(tok) || js_is_object(tok);
}

inline int js_is_empty(jstok_t *tok) {
  return !tok->first_child;
}

/*
 * JSON parser
 *****************************************************************************/

static jserr_t js_parse_string(jsparser_t *p, size_t t) {
  size_t start = p->pos, j;

  js_ensure_buf(p, 2);
  if (js(p)[0] != '"') return JS_EPARSE;
  p->pos++;

  for (; js(p)[0] != '\"'; p->pos++) {
    js_ensure_buf(p, 6);
    if (0 <= js(p)[0] && js(p)[0] < 32) return JS_EPARSE;

    if (js(p)[0] == '\\') {
      p->pos++;
      switch(js(p)[0]) {
        case '\"':
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
          continue;
        case 'u':
          for (j = 1; j < 5; j++)
            if (!is_hex_char(js(p)[j])) return JS_EPARSE;
          p->pos += 4;
          break;
        default:
          return JS_EPARSE;
      }
    }
  }

  if (js(p)[0] != '\"') return JS_EPARSE;
  else *((p->js)->buf + p->pos) = '\0';
  p->pos++;

  js_tok(p, t)->type = JS_STRING;
  js_tok(p, t)->start = start + 1;
  js_tok(p, t)->end = p->pos - 1;

  return 0;
}

static jserr_t js_parse_primitive(jsparser_t *p, size_t t) {
  size_t start = p->pos;
  jstype_t type;

  js_ensure_buf(p, 5);

  if (!strncmp(js(p), "null", 4)) {
    type = JS_NULL;
    p->pos += 4;
  } else if (!strncmp(js(p), "true", 4)) {
    type = JS_TRUE;
    p->pos += 4;
  } else if (!strncmp(js(p), "false", 5)) {
    type = JS_FALSE;
    p->pos += 5;
  } else {
    type = JS_NUMBER;

    if (js(p)[0] == '-') p->pos++;

    if (js(p)[0] == '0') p->pos++;
    else if(!js_scan_digits(p)) return JS_EPARSE;

    if (js(p)[0] == '.' && p->pos++ && !js_scan_digits(p))
      return JS_EPARSE;

    if (js(p)[0] == 'e' || js(p)[0] == 'E') {
      p->pos++;
      if (js(p)[0] == '+' || js(p)[0] == '-') p->pos++;
      if (!js_scan_digits(p)) return JS_EPARSE;
    }
  }

  js_tok(p, t)->type = type;
  js_tok(p, t)->start = start;
  js_tok(p, t)->end = p->pos;

  return 0;
}

static jserr_t js_parse_collection(jsparser_t *p, size_t t) {
  size_t err, key, val, prev = 0;
  char start, end;

  p->depth--;
  if (!p->depth) return JS_EPARSE;

  js_ensure_buf(p, 1);

  if ((start = js(p)[0]) == '[' || start == '{') {
    end = (start == '[') ? ']' : '}';
    js_tok(p, t)->type = (start == '[') ? JS_ARRAY : JS_OBJECT;
  } else {
    return JS_EPARSE;
  }

  p->pos++;

  while (1) {
    js_skip_ws(p);

    if (js(p)[0] == end) {
      p->pos++;
      p->depth++;
      break;
    } else {
      if (prev) {
        if (js(p)[0] != ',') return JS_EPARSE;
        p->pos++;
        js_skip_ws(p);
      }

      key = js_next_tok(p);
      val = js_next_tok(p);

      if (prev) js_tok(p, prev)->next_sibling = key;
      else js_tok(p, t)->first_child = key;

      js_tok(p, key)->parent = t;
      js_tok(p, key)->first_child = val;

      js_tok(p, val)->parent = key;

      switch (js_tok(p, t)->type) {
        case JS_ARRAY:
          js_tok(p, key)->type = JS_ITEM;
          js_tok(p, key)->idx = prev ? js_tok(p, prev)->idx + 1 : 0;
          break;
        case JS_OBJECT:
          if ((err = js_parse_string(p, key))) return err;
          js_tok(p, key)->type = JS_PAIR;
          js_skip_ws(p);
          if (js(p)[0] != ':') return JS_EPARSE;
          p->pos++;
          break;
        default:
          return JS_EBUG;
      }

      prev = key;

      if ((err = js_parse(p, val))) return err;
    }
  }

  return 0;
}

jserr_t js_parse(jsparser_t *p, size_t t) {
  js_skip_ws(p);

  switch (js(p)[0]) {
    case '[':
    case '{':
      return js_parse_collection(p, t);
    case '\"':
      return js_parse_string(p, t);
    default:
      return js_parse_primitive(p, t);
  }
}

jserr_t js_parse_one(jsparser_t *p, size_t *t) {
  js_skip_ws(p);
  return (js(p)[0] == '\0') ? JS_EDONE : js_parse(p, (*t = js_next_tok(p)));
}

/*
 * create JSON primitives
 *****************************************************************************/

size_t js_create_index(jsparser_t *p, size_t idx) {
  size_t item = js_next_tok(p);
  js_tok(p, item)->type = JS_ITEM;
  js_tok(p, item)->idx  = idx;
  return item;
}

/*
 * accessors
 *****************************************************************************/

size_t js_obj_get(jsparser_t *p, size_t obj, const char *key) {
  size_t v;
  jstok_t *tmp;
  for(v = js_tok(p, obj)->first_child; v; v = js_tok(p, v)->next_sibling) {
    tmp = js_tok(p, v);
    if (!strcmp(key, (p->js)->buf + tmp->start))
      return js_tok(p, v)->first_child;
  }
  return 0;
}

size_t js_obj_get_fuzzy(jsparser_t *p, size_t obj, const char *key) {
  size_t v, len, keylen = strlen(key);
  jstok_t *tmp;
  for(v = js_tok(p, obj)->first_child; v; v = js_tok(p, v)->next_sibling) {
    tmp = js_tok(p, v);
    len = tmp->end - tmp->start;
    if (len >= keylen && !strncasecmp(key, (p->js)->buf + tmp->start, keylen))
      return js_tok(p, v)->first_child;
  }
  return 0;
}

size_t js_array_get(jsparser_t *p, size_t ary, size_t idx) {
  size_t i, v;
  if (idx == SIZE_MAX) return 0;
  v = js_tok(p, ary)->first_child;
  for (i = 0; i < idx && v; i++)
    v = js_tok(p, v)->next_sibling;
  return i == idx ? js_tok(p, v)->first_child : 0;
}

/*
 * JSON printer
 *****************************************************************************/

jserr_t js_print(jsparser_t *p, size_t t, Buffer *b, int json) {
  jstok_t *tok = js_tok(p, t);
  char digitbuf[10];
  size_t v;

  json = json ? 1 : (js_is_collection(js_tok(p, t)) ? 1 : 0);

  switch (tok->type) {
    case JS_ARRAY:
      buf_append(b, "[", 1);
      v = tok->first_child;
      while (v) {
        if (v != tok->first_child)
          buf_append(b, ",", 1);
        js_print(p, v, b, json);
        v = js_tok(p, v)->next_sibling;
      }
      buf_append(b, "]", 1);
      break;
    case JS_OBJECT:
      buf_append(b, "{", 1);
      v = tok->first_child;
      while (v) {
        if (v != tok->first_child)
          buf_append(b, ",", 1);
        js_print(p, v, b, json);
        v = js_tok(p, v)->next_sibling;
      }
      buf_append(b, "}", 1);
      break;
    case JS_PAIR:
      if (!json) {
        buf_append(b, (p->js)->buf + tok->start, tok->end - tok->start);
      } else {
        buf_append(b, "\"", 1);
        buf_append(b, (p->js)->buf + tok->start, tok->end - tok->start);
        buf_append(b, "\":", 2);
        js_print(p, tok->first_child, b, json);
      }
      break;
    case JS_ITEM:
      if (!json) {
        snprintf(digitbuf, 10, "%zu", tok->idx);
        buf_append(b, digitbuf, strlen(digitbuf));
      } else {
        js_print(p, tok->first_child, b, json);
      }
      break;
    case JS_STRING:
      if (json) buf_append(b, "\"", 1);
      buf_append(b, (p->js)->buf + tok->start, tok->end - tok->start);
      if (json) buf_append(b, "\"", 1);
      break;
    case JS_NULL:
    case JS_TRUE:
    case JS_FALSE:
    case JS_NUMBER:
      buf_append(b, (p->js)->buf + tok->start, tok->end - tok->start);
      break;
    default:
      return JS_EBUG;
  }

  return 0;
}

jserr_t js_print_info(jsparser_t *p, size_t t, Buffer *b) {
  jstok_t *tmp;
  size_t v;

  switch (js_tok(p, t)->type) {
    case JS_OBJECT:
      v = js_tok(p, t)->first_child;
      while (v) {
        tmp = js_tok(p, v);
        buf_append(b, (p->js)->buf + tmp->start, tmp->end - tmp->start);
        if ((v = js_tok(p, v)->next_sibling)) buf_append(b, "\n", 1);
      }
      break;
    case JS_ARRAY:
      buf_append(b, "[array]", strlen("[array]"));
      break;
    case JS_STRING:
      buf_append(b, "[string]", strlen("[string]"));
      break;
    case JS_NUMBER:
      buf_append(b, "[number]", strlen("[number]"));
      break;
    case JS_TRUE:
    case JS_FALSE:
      buf_append(b, "[boolean]", strlen("[boolean]"));
      break;
    case JS_NULL:
      buf_append(b, "[null]", strlen("[null]"));
      break;
    case JS_NONE:
      buf_append(b, "[none]", strlen("[none]"));
      break;
    default:
      return JS_EBUG;
  }

  return 0;
}

/*
 * json string unescaping
 *****************************************************************************/

static unsigned long js_read_u_escaped(char **s) {
  unsigned long p;
  if (**s == '\\') *s += 2; // skip the \u if necessary
  sscanf(*s, "%4lx", &p);
  *s += 4;
  return p;
}

static unsigned long js_read_code_point(char **s) {
  unsigned long cp = js_read_u_escaped(s);
  return (cp >= 0xd800 && cp <= 0xdfff) // utf-16 surrogate pair
    ? (((cp - 0xd800) & 0x3ff) << 10 | ((js_read_u_escaped(s) - 0xdc00) & 0x3ff)) + 0x10000
    : cp;
}

static unsigned long utf_tag[4] = { 0x00, 0xc0, 0xe0, 0xf0 };

static void js_encode_u_escaped(char **in, char **out) {
  unsigned long p = js_read_code_point(in);
  int width = (p < 0x80) ? 1 : (p < 0x800) ? 2 : (p < 0x10000) ? 3 : 4;
  *out += width;
  switch (width) {
    case 4: *--(*out) = ((p | 0x80) & 0xbf); p >>= 6;
    case 3: *--(*out) = ((p | 0x80) & 0xbf); p >>= 6;
    case 2: *--(*out) = ((p | 0x80) & 0xbf); p >>= 6;
    case 1: *--(*out) =  (p | utf_tag[width - 1]);
  }
  *out += width;
}

char *js_unescape_string(char *in) {
  char *inp = in, *outp, *out;
  outp = out = jmalloc(strlen(in) + 1);

  while (*inp != '\0') {
    if (*inp != '\\') {
      *(outp++) = *(inp++);
    } else {
      switch(*(++inp)) {
        case 'b': *(outp++) = '\b'; inp++; break;
        case 'f': *(outp++) = '\f'; inp++; break;
        case 'n': *(outp++) = '\n'; inp++; break;
        case 'r': *(outp++) = '\r'; inp++; break;
        case 't': *(outp++) = '\t'; inp++; break;
        case 'u': inp++; js_encode_u_escaped(&inp, &outp); break;
        default:  *(outp++) = *(inp++);
      }
    }
  }

  *outp = '\0';

  return out;
}

/*
 * parser housekeeping
 *****************************************************************************/

#define MAX_DEPTH 20

void js_reset(jsparser_t *p) {
  p->curtok = 1;
  buf_reset(p->js, p->pos);
  p->pos = 0;
  p->depth = MAX_DEPTH;
}

void js_alloc(jsparser_t **p, FILE *in, size_t toks_size) {
  *p = jmalloc(sizeof(jsparser_t));
  buf_alloc(&((*p)->js));
  (*p)->in = in;
  (*p)->toks = jmalloc(sizeof(jstok_t) * toks_size);
  (*p)->toks_size = toks_size;
  js_reset(*p);
}

void js_free(jsparser_t **p) {
  buf_free(&((*p)->js));
  free((*p)->toks);
  free(*p);
  *p = NULL;
}
