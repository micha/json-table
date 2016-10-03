/*
 * jt: transform JSON into tabular data
 *****************************************************************************/

#define _GNU_SOURCE

#include "stack.c"
#include "buffer.c"
#include "js.c"
#include "util.c"

#define STACKSIZE 256
#define JT_VERSION "3.0.0"

int left_join = 1;

Buffer *OUTBUF;

Stack *DAT;
Stack *OUT;
Stack *SUB;
Stack *ITR;
Stack *IDX;

/*
 * interpreter
 *****************************************************************************/

int run(jsparser_t *p, int argc, char *argv[]) {
  size_t d = stack_head(DAT), tmp, itr;
  int e = 0, cols = 0;

  if (argc <= 0) return 0;

  if (js_is_array(js_tok(p, d))
      || (e = (js_is_object(js_tok(p, d)) && ! strcmp(argv[0], ".")))) {
    if (!js_is_collection(js_tok(p, d)) || js_is_empty(js_tok(p, d))) {
      stack_push(DAT, 0);
      run(p, argc - e, argv + e);
      if (! left_join) cols = -STACKSIZE;
    } else {
      if (stack_depth(ITR)) {
        itr = (size_t) stack_head(ITR);
        stack_pop(ITR);
      } else {
        itr = js_tok(p, d)->first_child;
      }

      stack_push(IDX, itr);
      stack_push(DAT, js_tok(p, itr)->first_child);
      cols = run(p, argc - e, argv + e);
      stack_pop(IDX);

      if (! stack_depth(ITR)) itr = js_tok(p, itr)->next_sibling;

      if (itr) stack_push(ITR, itr);
    }
    return cols;
  }

  else if (! strcmp(argv[0], "^")) {
    cols = 1;
    stack_push(OUT, stack_head(IDX));
  }

  else if (! strcmp(argv[0], "@")) {
    switch (js_tok(p, d)->type) {
      case JS_OBJECT:
        js_print_keys(p, d, OUTBUF);
        printf("%s\n", OUTBUF->buf);
        break;
      case JS_ARRAY:
        printf("[array]\n");
        break;
      case JS_STRING:
        printf("[string]\n");
        break;
      case JS_NUMBER:
        printf("[number]\n");
        break;
      case JS_TRUE:
      case JS_FALSE:
        printf("[boolean]\n");
        break;
      case JS_NULL:
        printf("[null]\n");
        break;
      case JS_NONE:
        printf("[none]\n");
        break;
      default:
        break;
    }
    exit(0);
  }

  else if (! strcmp(argv[0], "%")) {
    cols = 1;
    stack_push(OUT, d);
  }

  else if (! strcmp(argv[0], "[")) {
    stack_push(SUB, stack_depth(DAT));
  }

  else if (! strcmp(argv[0], "]")) {
    while (stack_depth(DAT) != stack_head(SUB))
      stack_pop(DAT);
    stack_pop(SUB);
  }

  else if (argv[0][0] == '~') {
    if (js_is_object(js_tok(p, d))) {
      tmp = js_obj_get_fuzzy(p, d, argv[0] + 1);
      if (! (tmp || left_join)) return -STACKSIZE;
      stack_push(DAT, tmp);
    } else {
      if (! left_join) return -STACKSIZE;
      stack_push(DAT, 0);
    }
  }

  else {
    if (js_is_collection(js_tok(p, d))) {
      if ((e = (argv[0][0] == '[')))
        argv[0][strlen(argv[0])] = '\0';
      tmp = js_is_object(js_tok(p, d))
        ? js_obj_get(p, d, argv[0] + e)
        : js_array_get(p, d, strtosizet(argv[0] + e));
      if (! (tmp || left_join)) return -STACKSIZE;
      stack_push(DAT, tmp);
    } else {
      if (! left_join) return -STACKSIZE;
      stack_push(DAT, 0);
    }
  }

  return cols + run(p, argc - 1, argv + 1);
}

/*
 * main
 *****************************************************************************/

void usage(int status) {
  fprintf(stderr, "jt %s - transform JSON data into tab delimited lines of text.\n\n", JT_VERSION);
  fprintf(stderr, "Usage: jt -h\n");
  fprintf(stderr, "       jt -V\n");
  fprintf(stderr, "       jt -u <string>\n");
  fprintf(stderr, "       jt [-j] [COMMAND ...]\n\n");
  fprintf(stderr, "Where COMMAND is one of `[', `]', `%%', `@', `.', `^', or a property name.\n");
  exit(status);
}

void version() {
  fprintf(stdout, "jt %s\n", JT_VERSION);
  fprintf(stdout, "Copyright © 2016 Micha Niskin\n");
  fprintf(stdout, "License EPL v1.0 <https://www.eclipse.org/legal/epl-v10.html>.\n");
  fprintf(stdout, "Source code available <https://github.com/micha/json-table>.\n");
  fprintf(stdout, "This is free software: you are free to change and redistribute it.\n");
  fprintf(stdout, "There is NO WARRANTY, to the extent permitted by law.\n");
  exit(0);
}

int main(int argc, char *argv[]) {
  size_t root = 0, item, idx = 0;
  jsparser_t *p;
  jserr_t err;
  int opt, cols, i;

  while ((opt = getopt(argc, argv, "+hVju:")) != -1) {
    switch (opt) {
      case 'h':
        usage(0);
        break;
      case 'V':
        version();
        break;
      case 'j':
        left_join = 0;
        break;
      case 'u':
        printf("%s", js_unescape_string((char*) strdup(optarg)));
        exit(0);
      default:
        fprintf(stderr, "\n");
        usage(1);
    }
  }

  if (argc - optind == 0) usage(0);

  stack_alloc(&DAT, "data",     STACKSIZE);
  stack_alloc(&OUT, "output",   STACKSIZE);
  stack_alloc(&SUB, "gosub",    STACKSIZE);
  stack_alloc(&ITR, "iterator", STACKSIZE);
  stack_alloc(&IDX, "index",    STACKSIZE);

  buf_alloc(&OUTBUF);

  js_alloc(&p, stdin, 128);

  while ((err = js_parse_one(p, &root)) != JS_EDONE) {
    if (err) die("can't parse JSON");

    item = js_next_tok(p);
    js_tok(p, item)->type = JS_ITEM;
    js_tok(p, item)->idx  = idx++;

    stack_push(IDX, item);
    stack_push(DAT, root);

    do {
      cols = run(p, argc - optind, argv + optind);

      if (cols > 0) {
        for (i = 0; i <= OUT->head; i++) {
          if ((item = (OUT->items)[i])) js_print(p, item, OUTBUF, 0);
          if (i < OUT->head) buf_append(OUTBUF, "\t", 1);
        }
        printf("%s\n", OUTBUF->buf);
      }

      stack_pop_to(DAT, 0);
      stack_pop_to(OUT, -1);
      stack_pop_to(SUB, -1);
      buf_reset(OUTBUF, 0);
    } while (stack_depth(ITR) > 0);

    js_reset(p);
    stack_pop_to(DAT, -1);
  }

  return 0;
}
