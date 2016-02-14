#define _GNU_SOURCE
#define JSMN_STRICT
#define JSMN_PARENT_LINKS

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jsmn.c"

char *colsep     = "\t";
char *rowsep     = "\n";
int  left_join   = 1;

/*
 * helpers
 *****************************************************************************/

void warn(const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "jt: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
}

void die(const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "jt: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(1);
}

void die_err(const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "jt: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  if (errno != 0)
    fprintf(stderr, ": %s", strerror(errno));
  fprintf(stderr, "\n");
  exit(1);
}

void die_mem() {
  die("can't allocate memory");
}

void *jmalloc(size_t size) {
  void *ret;
  if (! (ret = malloc(size))) die_mem();
  return ret;
}

void *jrealloc(void *ptr, size_t size) {
  void *ret;
  if (! (ret = realloc(ptr, size))) die_mem();
  return ret;
}

char *str(const char *fmt, ...) {
  char *s;
  va_list ap;
  va_start(ap, fmt);
  if (vasprintf(&s, fmt, ap) < 0) die_mem();
  va_end(ap);
  return s;
}

FILE *open(const char *path, const char *mode) {
  FILE *ret;
  if (! (ret = fopen(path, mode)))
    die_err("can't open %s", path);
  return ret;
}

char *read_stream(FILE *in) {
  char    buf[BUFSIZ];
  char    *ret         = NULL;
  size_t  ret_size     = 0;
  size_t  ret_newsize  = 0;
  size_t  ret_capacity = BUFSIZ * 2.5;
  ssize_t bytes_r      = NULL;
  int     fd           = fileno(in);

  ret = jmalloc(ret_capacity);

  for (;;) {
    bytes_r = read(fd, buf, sizeof(buf));

    if (bytes_r < 0)
      die_err("can't read input");
    else if (bytes_r == 0)
      return ret;

    if ((ret_newsize = ret_size + bytes_r) >= ret_capacity)
      ret = jrealloc(ret, (ret_capacity *= 2.5));

    memcpy(&ret[ret_size], buf, bytes_r);
    ret_size = ret_newsize;
    ret[ret_size] = '\0';
  }

  return ret;
}

void read_line(char **s, size_t *n, FILE *in) {
  if (getline(s, n, in) < 0) {
    if (feof(in)) exit(0);
    else die_err("can't read input");
  }
}

/*
 * stacks
 *****************************************************************************/

#define STACKSIZE 128

typedef struct Stack {
  void **items;
  char *name;
  int head;
} Stack;

void stack_push(Stack *stack, void *item) {
  if (stack->head >= STACKSIZE)
    die("maximum %s stack depth exceeded", stack->name);
  stack->items[(stack->head += 1)] = item;
}

void stack_pop(Stack *stack) {
  if ((stack->head -= 1) < -1)
    die("%s stack underflow", stack->name);
}

void stack_pop_to(Stack *stack, int n) {
  if (n < -1) die("%s stack underflow", stack->name);
  else if (n >= STACKSIZE) die("%s stack overflow", stack->name);
  stack->head = n;
}

void *stack_head(Stack stack) {
  return stack.head < 0 ? NULL : stack.items[stack.head];
}

int stack_depth(Stack stack) {
  return stack.head + 1;
}

void  *in_i[STACKSIZE];
void *out_i[STACKSIZE];
void  *go_i[STACKSIZE];
void *idx_i[STACKSIZE];
void *itr_i[STACKSIZE];

Stack IN  = { in_i,  "data",     -1 };
Stack OUT = { out_i, "output",   -1 };
Stack GO  = { go_i,  "gosub",    -1 };
Stack IDX = { idx_i, "index",    -1 };
Stack ITR = { itr_i, "iterator", -1 };

/*
 * json
 *****************************************************************************/

#define TOKSIZE 2

int parse_string(char *js, jsmntok_t **tok, size_t *toks) {
  int r;
  jsmn_parser p;
  size_t jslen = strlen(js);

  jsmn_init(&p);

  if (*toks == 0)
    *tok = jmalloc(sizeof(**tok) * (*toks = TOKSIZE));

  while ((r = jsmn_parse(&p, js, jslen, *tok, *toks)) < 0) {
    if (r != JSMN_ERROR_NOMEM) return r;
    else *tok = jrealloc(*tok, sizeof(**tok) * ((*toks) *= 2));
  }

  return 0;
}

int jnext(jsmntok_t *tok) {
  int i, j = 0;
  for (i = 0; i < tok->size; i++)
    j += jnext(tok + 1 + j);
  return j + 1;
}

jsmntok_t *toknext(jsmntok_t *tok) {
  int i, j = 0;
  for (i = 0; i < tok->size; i++)
    j += jnext(tok + 1 + j);
  return tok + 1 + j;
}

int isnull(const char *js, jsmntok_t *tok) {
  return tok->type == JSMN_PRIMITIVE && *(js + tok->start) == 'n';
}

char *jstr(const char *js, jsmntok_t *tok) {
  return (! tok || isnull(js, tok))
    ? str("")
    : str("%.*s", tok->end - tok->start, js + tok->start);
}

void jprint(const char *fmt, const char *js, jsmntok_t *tok) {
  char *s = jstr(js, tok);
  fprintf(stderr, fmt, s);
  free(s);
}

void obj_print_keys(const char *js, jsmntok_t *tok) {
  int i;
  jsmntok_t *v = tok + 1;
  if (tok->type != JSMN_OBJECT) die("not an object");
  for (i = 0; i < tok->size; i++, v = toknext(v))
    jprint("%s\n", js, v);
}

jsmntok_t *obj_get(const char *js, jsmntok_t *tok, const char *k) {
  int i;
  jsmntok_t *v;

  if (tok->type == JSMN_OBJECT)
    for (i = 0, v = tok + 1; i < tok->size; i++, v = toknext(v))
      if (! strncmp(k, js + v->start, v->end - v->start))
        return v + 1;

  return NULL;
}

jsmntok_t *ary_get(const char *js, jsmntok_t *tok, int n) {
  int i;
  jsmntok_t *v;

  if (n < 0 || n >= tok->size)
    die("array index out of bounds: %d", n);

  for (i = 0, v = tok + 1; i < n; i++)
    v = toknext(v);

  return v;
}

/*
 * runner
 *****************************************************************************/

int run(char *js, int argc, char *argv[]) {
  jsmntok_t *data = stack_head(IN);
  int *i, *j, cols = 0;

  if (data && data->type == JSMN_ARRAY) {
    if (data->size == 0) {
      stack_push(&IN, NULL);
      run(js, argc, argv);
    } else {
      i = (int*) stack_head(ITR);

      if (i) {
        stack_pop(&ITR);
      } else {
        i = jmalloc(sizeof(int*));
        *i = 0;
      }

      stack_push(&IN, ary_get(js, data, *i));
      stack_push(&IDX, i);
      cols = run(js, argc, argv);
      stack_pop(&IDX);

      if (! stack_head(ITR)) (*i)++;
      if (*i < data->size) stack_push(&ITR, i);
    }
    return cols;
  }

  if (argc <= 0) return 0;

  if (! strcmp(argv[0], "?")) {
    if (data && data->type == JSMN_OBJECT)
      obj_print_keys(js, data);
    exit(0);
  }

  if (! strcmp(argv[0], "%")) {
    if (data && ! isnull(js, data)) cols = 1;
    stack_push(&OUT, jstr(js, data));
  }

  else if (! strcmp(argv[0], "^")) {
    if (! (i = stack_head(IDX)))
      die("no index to print");
    cols = 1;
    stack_push(&OUT, str("%d", * (int*) i));
  }

  else if (! strcmp(argv[0], "[")) {
    stack_push(&GO, stack_head(IN));
  }

  else if (! strcmp(argv[0], "]")) {
    while (stack_head(IN) != stack_head(GO))
      stack_pop(&IN);
    stack_pop(&GO);
  }

  else if (data) {
    data = obj_get(js, data, argv[0]);
    if (! (data || left_join)) return -1 * STACKSIZE;
    stack_push(&IN, data);
  }

  return cols + run(js, argc - 1, argv + 1);
}

/*
 * main
 *****************************************************************************/

void usage(int status) {
  fprintf(stderr, "Usage: jt [-h] [-F <char>] [-R <char>] [COMMAND ...]\n");
  fprintf(stderr, "Transform JSON data into tab delimited lines of text.\n");
  fprintf(stderr, "COMMAND is one of `[', `]', `%%', `?', `^', or a property name.");
  exit(status);
}

int main(int argc, char *argv[]) {
  jsmntok_t *tok;
  size_t    jslen, toks;
  char      *js, **o;
  int       opt, cols, streaming, line, i;
  FILE      *infile, *outfile;

  streaming = 0;
  infile    = stdin;
  outfile   = stdout;

  while ((opt = getopt(argc, argv, "+hjsi:o:F:R:")) != -1) {
    switch (opt) {
      case 'h':
        usage(0);
        break;
      case 'j':
        left_join = 0;
        break;
      case 's':
        streaming = 1;
        break;
      case 'i':
        infile = open((char*) strdup(optarg), "r");
        break;
      case 'o':
        outfile = open((char*) strdup(optarg), "w");
        break;
      case 'F':
        colsep = (char*) strdup(optarg);
        break;
      case 'R':
        rowsep = (char*) strdup(optarg);
        break;
      default:
        fprintf(stderr, "\n");
        usage(1);
    }
  }

  if (argc - optind == 0) usage(0);

  jslen = 0;
  js    = NULL;
  toks  = 0;
  tok   = NULL;
  line  = 1;
  o     = (char **)(OUT.items);

  do {
    if (streaming) read_line(&js, &jslen, infile);
    else js = read_stream(infile);

    if (parse_string(js, &tok, &toks)) {
      if (! streaming) die("can't parse json");
      else warn("can't parse json: line %d -- continuing\n", line);
    } else {
      stack_push(&IN, tok);

      do {
        cols = run(js, argc - optind, argv + optind);

        if (cols > 0)
          for (i = 0; i <= OUT.head; i++)
            fprintf(outfile, "%s%s", o[i], i < OUT.head ? colsep : rowsep);

        for (i = 0; i <= OUT.head; i++)
          free(o[i]);

        stack_pop_to(&IN, 0);
        stack_pop_to(&OUT, -1);
      } while (stack_depth(ITR) > 0);

      stack_pop_to(&IN, -1);
    }
  } while (streaming && ++line);

  return 0;
}
