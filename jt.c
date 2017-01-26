/*
 * jt: transform JSON into tabular data
 *****************************************************************************/

#include "stack.h"
#include "buffer.h"
#include "js.h"
#include "util.h"

#define STACKSIZE 256
#define JT_VERSION "4.0.2"

int left_join = 1;
int auto_iter = 1;

Buffer *OUTBUF;

Stack *DAT;
Stack *OUT;
Stack *SUB;
Stack *ITR;
Stack *IDX;

typedef struct {
  char cmd;
  char *text;
} word_t;

/*
 * interpreter
 *****************************************************************************/

int run(jsparser_t *p, int wordc, word_t *wordv) {
  size_t d = stack_head(DAT), tmp, itr;
  int e = 0, cols = 0;

  if (wordc <= 0) return 0;

  if ((js_is_array(js_tok(p, d)) && auto_iter) || (e = (wordv[0].cmd == '.'))) {
    if (!js_is_collection(js_tok(p, d)) || js_is_empty(js_tok(p, d))) {
      stack_push(DAT, 0);
      run(p, wordc - e, wordv + e);
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
      cols = run(p, wordc - e, wordv + e);
      stack_pop(IDX);

      if (! stack_depth(ITR)) itr = js_tok(p, itr)->next_sibling;

      if (itr) stack_push(ITR, itr);
    }
    return cols;
  } else {
    switch (wordv[0].cmd) {
      case '^':
        cols = 1;
        stack_push(OUT, stack_head(IDX));
        break;
      case '%':
        cols = 1;
        stack_push(OUT, d);
        break;
      case '[':
        stack_push(SUB, stack_depth(DAT));
        break;
      case ']':
        while (stack_depth(DAT) != stack_head(SUB))
          stack_pop(DAT);
        stack_pop(SUB);
        break;
      case '\0':
        if (js_is_collection(js_tok(p, d))) {
          tmp = js_is_object(js_tok(p, d))
            ? js_obj_get(p, d, wordv[0].text)
            : js_array_get(p, d, strtosizet(wordv[0].text));
          if (! (tmp || left_join)) return -STACKSIZE;
          stack_push(DAT, tmp);
        } else {
          if (! left_join) return -STACKSIZE;
          stack_push(DAT, 0);
        }
        break;
      case '@':
        js_print_info(p, d, OUTBUF);
        printf("%s\n", OUTBUF->buf);
        exit(0);
      default:
        die("unexpected command");
    }
  }

  return cols + run(p, wordc - 1, wordv + 1);
}

/*
 * main
 *****************************************************************************/

void usage(int status) {
  fprintf(stderr, "jt %s - transform JSON data into tab delimited lines of text.\n\n", JT_VERSION);
  fprintf(stderr, "Usage: jt -h\n");
  fprintf(stderr, "       jt -V\n");
  fprintf(stderr, "       jt -u <string>\n");
  fprintf(stderr, "       jt [-aj] [COMMAND ...]\n\n");
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

void parse_commands(int argc, char *argv[], word_t *wordv) {
  int i, len, e;
  for (i = 0; i < argc; i++) {
    len = strlen(argv[i]);
    if (len == 1) {
      switch(argv[i][0]) {
        case '[': case ']': case '%': case '^': case '@': case '.':
          wordv[i].cmd = argv[i][0];
          break;
        default:
          wordv[i].cmd = '\0';
          wordv[i].text = argv[i];
      }
    } else {
      wordv[i].cmd = '\0';
      if ((e = (argv[i][0] == '[' && argv[i][len - 1] == ']')))
        argv[i][len -1] = '\0';
      wordv[i].text = argv[i] + e;
    }
  }
}

int main(int argc, char *argv[]) {
  size_t root = 0, item, idx = 0, wordc;
  word_t *wordv;
  jsparser_t *p;
  jserr_t err;
  int opt, cols, i;

  while ((opt = getopt(argc, argv, "+hVajsu:")) != -1) {
    switch (opt) {
      case 'h':
        usage(0);
        break;
      case 'V':
        version();
        break;
      case 'a':
        auto_iter = 0;
        break;
      case 'j':
        left_join = 0;
        break;
      case 's':
        /* for compatibility -- this option is now redundant */
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

  wordc = argc - optind;
  wordv = malloc(sizeof(word_t) * argc - optind);
  parse_commands(argc - optind, argv + optind, wordv);

  stack_alloc(&DAT, "data",     STACKSIZE);
  stack_alloc(&OUT, "output",   STACKSIZE);
  stack_alloc(&SUB, "gosub",    STACKSIZE);
  stack_alloc(&ITR, "iterator", STACKSIZE);
  stack_alloc(&IDX, "index",    STACKSIZE);

  buf_alloc(&OUTBUF);

  js_alloc(&p, stdin, 128);

  while ((err = js_parse_one(p, &root)) != JS_EDONE) {
    if (err) die("can't parse JSON");

    item = js_create_index(p, idx++);

    stack_push(IDX, item);
    stack_push(DAT, root);

    do {
      cols = run(p, wordc, wordv);

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
    stack_pop_to(IDX, -1);
  }

  /*
  js_free(&p);

  buf_free(&OUTBUF);

  stack_free(&DAT);
  stack_free(&OUT);
  stack_free(&SUB);
  stack_free(&ITR);
  stack_free(&IDX);

  free(wordv);
  */

  return 0;
}
