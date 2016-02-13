#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jansson.h>

char *colsep     = "\t";
char *rowsep     = "\n";

void die(const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "jt: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(1);
}

void die_mem() {
  die("failed to allocate memory");
}

char* read_stream(FILE *in) {
  char    buf[BUFSIZ];
  char    *ret         = NULL;
  size_t  ret_size     = 0;
  size_t  ret_newsize  = 0;
  size_t  ret_capacity = BUFSIZ * 2.5;
  ssize_t bytes_r      = NULL;
  int     fd           = fileno(in);

  if(! (ret = malloc(ret_capacity))) die_mem();

  for (;;) {
    bytes_r = read(fd, buf, sizeof(buf));

    if (bytes_r < 0)
      die("read stdin failed");
    else if (bytes_r == 0)
      return ret;

    if ((ret_newsize = ret_size + bytes_r) >= ret_capacity)
      if (! (ret = realloc(ret, (ret_capacity *= 2.5)))) die_mem();

    memcpy(&ret[ret_size], buf, bytes_r);
    ret_size = ret_newsize;
    ret[ret_size] = '\0';
  }

  return ret;
}

/*
 * stacks
 *****************************************************************************/

#define STACKSIZE 128

typedef struct Stack {
  void** items;
  int    head;
} Stack;

Stack *stack_new() {
  void **items = malloc(STACKSIZE);
  Stack *stack = malloc(sizeof(Stack));
  if (! items || ! stack) die_mem();
  stack->head = -1;
  stack->items = items;
  return stack;
}

void stack_push(Stack *stack, void *item) {
  if (stack->head >= STACKSIZE)
    die("maximum stack depth exceeded");
  stack->items[(stack->head += 1)] = item;
}

void stack_pop(Stack *stack) {
  if ((stack->head -= 1) < -1)
    die("stack underflow");
}

void *stack_head(Stack *stack) {
  return stack->head < 0 ? NULL : stack->items[stack->head];
}

void *stack_nth(Stack *stack, int i) {
  return stack->head <= i ? NULL : stack->items[i];
}

int stack_depth(Stack *stack) {
  return stack->head + 1;
}

/*
 * iterators
 *****************************************************************************/

typedef struct Iter {
  void *val;
  int  len;
  int  pos;
} Iter;

void *iter_peek(Iter *it) {
  return it->val + it->pos;
}

void iter_next(Iter *it) {
  if (it->pos++ >= it->len)
    die("index out of bounds");
}

void iter_reset(Iter *it) {
  it->pos = 0;
}

int iter_nexts(Iter *it) {
  return (it->len - 1) - it->pos;
}

int iter_fin(Iter *it) {
  return iter_nexts(it) < 0;
}

int iter_hasnext(Iter *it) {
  return iter_nexts(it) > 0;
}

/*
 * functions
 *****************************************************************************/

void print_keys(json_t *obj) {
  const char *k;
  json_t *v;
  json_object_foreach(obj, k, v)
    fprintf(stderr, "%s\n", k);
  fflush(stderr);
  exit(0);
}

void print_atom(json_t *x) {
  int jtype = json_typeof(x);
  switch (jtype) {
    case JSON_STRING:
      printf("%s", json_string_value(x));
      break;
    case JSON_INTEGER:
      printf("%" JSON_INTEGER_FORMAT, json_integer_value(x));
      break;
    case JSON_REAL:
      printf("%f", json_real_value(x));
      break;
    case JSON_TRUE:
      printf("true");
      break;
    case JSON_FALSE:
      printf("false");
      break;
    case JSON_NULL:
      printf("%s", "");
      break;
    default:
      die("unknown json type: %d", jtype);
  }
  fflush(stdout);
}

void do_gosub(Stack *input, Stack *gosub) {
  stack_push(gosub, stack_head(input));
}

void do_return(Stack *input, Stack *gosub) {
  if (! stack_head(gosub))
    die("return with no gosub on the stack");
  while (stack_head(input) != stack_head(gosub))
    stack_pop(input);
  stack_pop(gosub);
}

json_t *json_get(json_t *t, char *k) {
  if (json_typeof(t) != JSON_OBJECT)
    die("not an object, no such key: %s", k);
  json_t *ret = json_object_get(t, k);
  if (! ret) die("no such key: %s", k);
  return ret;
}

void run(Stack *in, Stack *out, Stack *go, Stack *idx, Stack *itr, Iter *arg) {
  json_t  *t = (json_t*) stack_head(in), *v, *w;
  char *k, *a = iter_fin(arg) ? NULL : ((char**) arg->val)[arg->pos];
  int i, j, ret, jtype = json_typeof(t);
  json_int_t *ix, *iy;

  if (jtype == JSON_ARRAY) {
    if (stack_depth(itr) > 0) {
      ix = stack_head(itr);
      stack_pop(itr);
      if (*ix < 0) *ix = 0;
    } else {
      if (! (ix = malloc(sizeof(json_int_t*)))) die_mem();
      *ix = 0;
    }
    stack_push(idx, (void*) ix);
    stack_push(in, json_array_get(t, *ix));
    run(in, out, go, idx, itr, arg);
    stack_pop(idx);
    if (! stack_depth(itr) || *((json_int_t*) stack_head(itr)) < 0)
      *ix = *ix + 1;
    if (*ix == json_array_size(t))
      *ix = -1;
    stack_push(itr, ix);
    return;
  } else if (! a) {
    return;
  } else if (! strcmp(a, "[")) {
    iter_next(arg);
    do_gosub(in, go);
  } else if (! strcmp(a, "]")) {
    iter_next(arg);
    do_return(in, go);
  } else if (! strcmp(a, "%")) {
    iter_next(arg);
    stack_push(out, t);
  } else if (! strcmp(a, "^")) {
    v = stack_head(idx);
    if (! v)
      die("no array index to print");
    iter_next(arg);
    stack_push(out, json_integer(* (json_int_t*) v));
  } else if (! strcmp(a, "@")) {
  } else if (! strcmp(a, "?")) {
    if (jtype == JSON_OBJECT)
      json_object_foreach(t, k, v)
        fprintf(stderr, "%s\n", k);
    else if (jtype == JSON_ARRAY)
      fprintf(stderr, "<<array: %d>>\n", (int) json_array_size(t));
    else if (jtype == JSON_STRING)
      fprintf(stderr, "<<string: %s>>\n", json_string_value(t));
    else
      fprintf(stderr, "<<primitive>>\n");
    exit(0);
  } else {
    iter_next(arg);
    stack_push(in, json_get(t, a));
  }

  run(in, out, go, idx, itr, arg);
}

/*
 * main
 *****************************************************************************/

#define OPTSPEC "+F:i:R:"

int main(int argc, char *argv[]) {
  int opt;
  FILE *infile = NULL;

  while ((opt = getopt(argc, argv, OPTSPEC)) != -1) {
    switch (opt) {
      case 'i':
        if (! (infile = fopen((char*) strdup(optarg), "r")))
          die("can't open file for reading: %s", optarg);
      case 'F':
        colsep = (char*) strdup(optarg);
        break;
      case 'R':
        rowsep = (char*) strdup(optarg);
        break;
    }
  }

  char  *json   = read_stream(infile ? infile : stdin);
  Stack *input  = stack_new();
  Stack *output = stack_new();
  Stack *gosub  = stack_new();
  Stack *index  = stack_new();
  Stack *depth  = stack_new();
  Iter   argv_i = { argv + optind, argc - 1, 0 };

  json_t *root, *itrp;
  json_int_t *ret;
  json_error_t error;
  if (! (root = json_loads(json, 0, &error)))
    die("json line %d: %s", error.line, error.text);
  free(json);

  stack_push(input, root);

  do {
    run(input, output, gosub, index, depth, &argv_i);
    input->head = 0; // implicit gosub - return
    iter_reset(&argv_i);

    for (int i = 0; i <= output->head; i++) {
      print_atom(((json_t**)output->items)[i]);
      printf("%s", i < output->head ? colsep : rowsep);
    }

    while (stack_depth(output) > 0)
      stack_pop(output);

    ret = stack_head(depth);
  } while (ret && *ret >= 0);

  return 0;
}
