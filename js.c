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

inline char *js_buf(jsparser_t *p, size_t t) {
  return (p->js)->buf + js_tok(p, t)->start;
}

inline size_t js_len(jsparser_t *p, size_t t) {
  return js_tok(p, t)->end - js_tok(p, t)->start;
}

static inline const char *js(jsparser_t *p) {
  return (p->js)->buf + p->pos;
}

static inline void init_tok(jstok_t *tok) {
  tok->type = JS_NONE;
  tok->start = 0;
  tok->end = 0;
  tok->idx = 0;
  tok->parent = 0;
  tok->first_child = 0;
  tok->next_sibling = 0;
  tok->parsed = 0;
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

inline int js_is_string(jstok_t *tok) {
  return tok->type == JS_STRING;
}

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

jserr_t js_print(jsparser_t *p, size_t t, Buffer *b, int json, int csv) {
  jstok_t *tok = js_tok(p, t);
  char digitbuf[10];
  size_t v;

  json = json ? 1 : (js_is_collection(js_tok(p, t)) ? 1 : 0);

  switch (tok->type) {
    case JS_ARRAY:
      buf_write(b, '[');
      v = tok->first_child;
      while (v) {
        if (v != tok->first_child)
          buf_write(b, ',');
        js_print(p, v, b, json, csv);
        v = js_tok(p, v)->next_sibling;
      }
      buf_write(b, ']');
      break;
    case JS_OBJECT:
      buf_write(b, '{');
      v = tok->first_child;
      while (v) {
        if (v != tok->first_child)
          buf_write(b, ',');
        js_print(p, v, b, json, csv);
        v = js_tok(p, v)->next_sibling;
      }
      buf_write(b, '}');
      break;
    case JS_PAIR:
      if (!json) {
        if (csv) {
          js_unescape_string(b, js_buf(p, t), js_len(p, t), csv);
        } else {
          buf_append(b, js_buf(p, t), js_len(p, t));
        }
      } else {
        buf_write(b, '\"');
        if (csv) {
          buf_write(b, '\"');
          buf_append_csv(b, js_buf(p, t), js_len(p, t));
          buf_write(b, '\"');
        } else {
          buf_append(b, js_buf(p, t), js_len(p, t));
        }
        buf_write(b, '\"');
        buf_write(b, ':');
        js_print(p, tok->first_child, b, json, csv);
      }
      break;
    case JS_ITEM:
      if (!json) {
        snprintf(digitbuf, 10, "%zu", tok->idx);
        buf_append(b, digitbuf, strlen(digitbuf));
      } else {
        js_print(p, tok->first_child, b, json, csv);
      }
      break;
    case JS_STRING:
      if (json) {
        buf_write(b, '\"');
        if (csv) buf_write(b, '\"');
      }
      if (csv) {
        if (json) {
          buf_append_csv(b, js_buf(p, t), js_len(p, t));
        } else {
          js_unescape_string(b, js_buf(p, t), js_len(p, t), csv);
        }
      } else {
        buf_append(b, (p->js)->buf + tok->start, tok->end - tok->start);
      }
      if (json) {
        buf_write(b, '\"');
        if (csv) buf_write(b, '\"');
      }
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
        if ((v = js_tok(p, v)->next_sibling)) buf_write(b, '\n');
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

static void js_encode_u_escaped(Buffer *b, char **in, int csv) {
  char buf[4];
  unsigned long p = js_read_code_point(in);
  int width = (p < 0x80) ? 1 : (p < 0x800) ? 2 : (p < 0x10000) ? 3 : 4;

  switch (width) {
    case 4: buf[3] = ((p | 0x80) & 0xbf); p >>= 6;
    case 3: buf[2] = ((p | 0x80) & 0xbf); p >>= 6;
    case 2: buf[1] = ((p | 0x80) & 0xbf); p >>= 6;
    case 1: buf[0] =  (p | utf_tag[width - 1]);
  }

  if (csv && width == 1 && buf[0] == '\"') {
    buf[1] = '\"';
    width++;
  }

  buf_append_unchecked(b, buf, width);
}

void js_unescape_string(Buffer *b, char *in, size_t len, int csv) {
  char *inp = in, *endp = in + len;

  buf_check(b, len);

  while (inp < endp && *inp != '\0') {
    if (*inp == '\"' || (0 <= *inp && *inp < 32))
      die("can't parse JSON");

    if (*inp != '\\') {
      buf_write_unchecked(b, *(inp++));
    } else {
      if (csv && *(inp + 1) == '\"')
        buf_write_unchecked(b, '\"');
      switch(*(++inp)) {
        case 'b': buf_write_unchecked(b, '\b'); inp++; break;
        case 'f': buf_write_unchecked(b, '\f'); inp++; break;
        case 'n': buf_write_unchecked(b, '\n'); inp++; break;
        case 'r': buf_write_unchecked(b, '\r'); inp++; break;
        case 't': buf_write_unchecked(b, '\t'); inp++; break;
        case 'u': inp++; js_encode_u_escaped(b, &inp, csv); break;
        case '\"': case '\\': case '/': buf_write_unchecked(b, *(inp++)); break;
        default: die("can't parse JSON");
      }
    }
  }
}

/*
 * parser housekeeping
 *****************************************************************************/

#define MAX_DEPTH 20

void js_save(jsparser_t *p, jsstate_t *s) {
  s->bufpos     = (p->js)->pos;
  s->parserpos  = p->pos;
}

void js_restore(jsparser_t *p, jsstate_t *s1, jsstate_t *s2) {
  buf_splice(p->js, s1->bufpos, s2->bufpos);
}

void js_reset(jsparser_t *p) {
  p->curtok = 1;
  buf_reset(p->js, p->pos);
  p->pos = 0;
  p->depth = MAX_DEPTH;
}

void js_alloc(jsparser_t **p, FILE *in, size_t toks_size) {
  *p = jmalloc(sizeof(jsparser_t));
  buf_alloc(&((*p)->js));
  (*p)->pos = 0;
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
