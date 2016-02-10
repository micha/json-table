#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <jansson.h>

char *colsep     = "\t";
char *rowsep     = "\n";
char *gosubarg   = "[";
char *returnarg  = "]";
char *explodearg = "@";

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

void die_json(json_error_t err) {
  fprintf(stderr, "jt: error: json line %d: %s\n", err.line, err.text);
}

char* read_stdin() {
  char    buf[BUFSIZ];
  char    *ret         = NULL;
  size_t  ret_size     = 0;
  size_t  ret_newsize  = 0;
  size_t  ret_capacity = BUFSIZ * 2.5;
  ssize_t bytes_r      = NULL;
  int     fd           = fileno(stdin);

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
  int    size;
} Stack;

Stack *stack_new() {
  void **items = malloc(STACKSIZE);
  Stack *stack = malloc(sizeof(Stack));
  if (! items || ! stack) die_mem();
  stack->size = STACKSIZE;
  stack->head = -1;
  stack->items = items;
  return stack;
}

void stack_push(Stack *stack, void *item) {
  int capacity = stack->size;
  if (stack->head >= capacity)
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

void print_keys_and_exit(json_t *obj) {
  const char *k;
  json_t *v;
  json_object_foreach(obj, k, v)
    fprintf(stderr, "%s\n", k);
  exit(0);
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
}

int is_collection(json_t *x) {
  int jtype = json_typeof(x);
  return jtype == JSON_ARRAY || jtype == JSON_OBJECT;
}

int run(Stack *input, Stack *gosub, Stack *output, Iter *argv, int iters) {
  json_t *y, *x = stack_head(input);
  char *arg = NULL;
  int jtype, tmp, size, idx, ret, ret2;

  if (! x) return 1;

  jtype = json_typeof(x);

  if (jtype == JSON_ARRAY) {
    size = json_array_size(x);
    if (size == 0) return 1;
    idx = (iters < 0) ? 0 : ((-iters % size) + size) % size;
    y = json_array_get(x, idx);
    if (! is_collection(y))
      stack_push(output, y);
    stack_push(input, y);
    return run(input, gosub, output, argv, iters) * size;
  }

  if (! iter_fin(argv))
    arg = ((char**)argv->val)[argv->pos];

  if (arg) {
    if (! strcmp(arg, gosubarg)) {
      do_gosub(input, gosub);
      iter_next(argv);
      return run(input, gosub, output, argv, iters);
    } else if (! strcmp(arg, returnarg)) {
      do_return(input, gosub);
      iter_next(argv);
      return run(input, gosub, output, argv, iters);
    } else if (jtype == JSON_OBJECT) {
      if (! (y = json_object_get(x, arg))) return 1;
      if (! is_collection(y))
        stack_push(output, y);
      iter_next(argv);
      stack_push(input, y);
      return run(input, gosub, output, argv, iters);
    } else {
      die("illegal operation: %s", arg);
    }
  } else if (jtype == JSON_OBJECT && output->head < 0) {
    print_keys_and_exit(x);
  } else {
    return 1;
  }
}

/*
 * main
 *****************************************************************************/

#define OPTSPEC "+F:R:"

int main(int argc, char *argv[]) {
  int opt;

  while ((opt = getopt(argc, argv, OPTSPEC)) != -1) {
    switch (opt) {
      case 'F':
        colsep = (char*) strdup(optarg);
        break;
      case 'R':
        rowsep = (char*) strdup(optarg);
        break;
    }
  }

  json_t *root;
  json_error_t error;
  char*  text   = read_stdin();
  Stack* input  = stack_new();
  Stack* gosub  = stack_new();
  Stack* output = stack_new();
  Iter   argv_i = { argv + optind, argc - 1, 0 };
  int    iters  = -1;
  int    ret    = -1;

  if (! (root = json_loads(text, 0, &error)))
    die("json line %d: %s", error.line, error.text);
  free(text);

  stack_push(input, root);

  do {
    ret = run(input, gosub, output, &argv_i, iters);
    input->head = 0; // implicit gosub - return
    iter_reset(&argv_i);

    for (int i = 0; i <= output->head; i++) {
      print_atom(((json_t**)output->items)[i]);
      printf("%s", i < output->head ? colsep : rowsep);
    }

    while (stack_depth(output) > 0)
      stack_pop(output);

    if (iters < 0) iters = ret;
  } while (iters-- > 0);

  return 0;
}
