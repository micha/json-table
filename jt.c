/*
 * jt: transform JSON into tabular data
 *****************************************************************************/

#include "stack.h"
#include "buffer.h"
#include "js.h"
#include "util.h"

#define STACKSIZE 256
#define JT_VERSION "4.3.1"

int opt_join = 0;
int opt_iter = 0;
int opt_csv  = 0;

FILE *devnull;
Buffer *OUTBUF;
jsparser_t *p;

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

int run(int wordc, word_t *wordv) {
  size_t d = stack_head(DAT), tmp, itr, root;
  int e = 0, cols = 0;

  if (wordc <= 0) return 0;

  if (d && ((js_is_array(js_tok(p, d)) && !opt_iter) || (e = (wordv[0].cmd == '.')))) {
    if (!js_is_collection(js_tok(p, d)) || js_is_empty(js_tok(p, d))) {
      stack_push(DAT, 0);
      run(wordc - e, wordv + e);
      if (opt_join) cols = -STACKSIZE;
    } else {
      if (stack_depth(ITR)) {
        itr = (size_t) stack_head(ITR);
        stack_pop(ITR);
      } else {
        itr = js_tok(p, d)->first_child;
      }

      stack_push(IDX, itr);
      stack_push(DAT, js_tok(p, itr)->first_child);
      cols = run(wordc - e, wordv + e);
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
      case '+':
        if (js_tok(p, d)->parsed) {
          stack_push(DAT, js_tok(p, d)->parsed);
        } else if (js_is_string(js_tok(p, d))) {
          js_unescape_string(p->js, js_buf(p, d), js_len(p, d), 0);
          if (! js_parse_one(p, &root)) {
            js_tok(p, d)->parsed = root;
            stack_push(DAT, root);
          }
        }
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
        if (d && js_is_collection(js_tok(p, d))) {
          tmp = js_is_object(js_tok(p, d))
            ? js_obj_get(p, d, wordv[0].text)
            : js_array_get(p, d, strtosizet(wordv[0].text));
          if (! (tmp || !opt_join)) return -STACKSIZE;
          stack_push(DAT, tmp);
        } else {
          if (opt_join) return -STACKSIZE;
          stack_push(DAT, 0);
        }
        break;
      case '@':
        js_print_info(p, d, OUTBUF);
        buf_println(OUTBUF);
        exit(0);
      default:
        die("unexpected command");
    }
  }

  return cols + run(wordc - 1, wordv + 1);
}

/*
 * helpers
 *****************************************************************************/

size_t parse_as_string(const char *s) {
  FILE *in = p->in;
  size_t t;

  p->in = devnull;

  buf_write(p->js, '\"');
  buf_append(p->js, s, strlen(s));
  buf_write(p->js, '\"');

  if (js_parse_one(p, &t))
    die("can't parse JSON");

  p->in = in;
  return t;
}

void print_tok(size_t t) {
  if (!t) return;
  if (opt_csv) buf_write(OUTBUF, '\"');
  js_print(p, t, OUTBUF, 0, opt_csv);
  if (opt_csv) buf_write(OUTBUF, '\"');
}

void print_stack(Stack *s) {
  for (int i = 0; i <= s->head; i++) {
    print_tok((s->items)[i]);
    if (i < s->head) buf_write(OUTBUF, opt_csv ? ',' : '\t');
  }
}

int parse_commands(int argc, char *argv[], word_t *wordv) {
  int i, len, e, have_headers = 0;
  for (i = 0; i < argc; i++) {
    len = strlen(argv[i]);
    if (len == 1) {
      switch(argv[i][0]) {
        case '[': case ']': case '@': case '.': case '+':
          wordv[i].cmd = argv[i][0];
          wordv[i].text = NULL;
          break;
        case '%': case '^':
          wordv[i].cmd = argv[i][0];
          wordv[i].text = NULL;
          stack_push(OUT, parse_as_string(""));
          break;
        default:
          wordv[i].cmd = '\0';
          wordv[i].text = argv[i];
      }
    } else if (len >= 2 && (argv[i][0] == '%' || argv[i][0] == '^') && argv[i][1] == '=') {
      wordv[i].cmd = argv[i][0];
      wordv[i].text = argv[i] + 2;
      stack_push(OUT, parse_as_string(wordv[i].text));
      have_headers = 1;
    } else {
      wordv[i].cmd = '\0';
      if ((e = (argv[i][0] == '[' && argv[i][len - 1] == ']')))
        argv[i][len -1] = '\0';
      wordv[i].text = argv[i] + e;
    }
  }
  return have_headers;
}

/*
 * main
 *****************************************************************************/

void usage() {
  fprintf(stderr, "jt %s - transform JSON data into tab delimited lines of text.\n\n", JT_VERSION);
  fprintf(stderr, "Usage: jt -h\n");
  fprintf(stderr, "       jt -V\n");
  fprintf(stderr, "       jt -u <string>\n");
  fprintf(stderr, "       jt [-acj] [COMMAND ...]\n\n");
  fprintf(stderr, "Where COMMAND is one of `[', `]', `%%', `@', `.', `^', `+', or a property name.\n");
  exit(0);
}

void version() {
  fprintf(stdout, "jt %s\n", JT_VERSION);
  fprintf(stdout, "Copyright © 2017 Micha Niskin\n");
  fprintf(stdout, "License EPL v1.0 <https://www.eclipse.org/legal/epl-v10.html>.\n");
  fprintf(stdout, "Source code available <https://github.com/micha/json-table>.\n");
  fprintf(stdout, "This is free software: you are free to change and redistribute it.\n");
  fprintf(stdout, "There is NO WARRANTY, to the extent permitted by law.\n");
  exit(0);
}

void unescape(char *s) {
  int len = strlen(s), quoted = (len > 2 && s[0] == '\"' && s[len - 1] == '\"');
  if (opt_csv) buf_write(OUTBUF, '\"');
  js_unescape_string(OUTBUF, quoted ? s+1 : s, quoted ? len-2 : len, opt_csv);
  if (opt_csv) buf_write(OUTBUF, '\"');
  buf_println(OUTBUF);
  exit(0);
}

int main(int argc, char *argv[]) {
  size_t root = 0, idx = 0, wordc = 0, bpos = 0, ppos = 0;
  word_t *wordv;
  jserr_t err;
  int opt;

  if (! (devnull = fopen("/dev/null", "r")))
    die_err("can't open /dev/null");

  buf_alloc(&OUTBUF);

  while ((opt = getopt(argc, argv, "+hVacjsu:")) != -1) {
    switch (opt) {
      case 'h': usage();          break;
      case 'V': version();        break;
      case 'u': unescape(optarg); break;
      case 'a': opt_iter = 1;     break;
      case 'c': opt_csv  = 1;     break;
      case 'j': opt_join = 1;     break;
      case 's': /* no-op */       break;
      default:  exit(1);
    }
  }

  if (argc - optind == 0) usage();

  wordc = argc - optind;
  wordv = malloc(sizeof(word_t) * argc - optind);

  stack_alloc(&DAT, "data",     STACKSIZE);
  stack_alloc(&OUT, "output",   STACKSIZE);
  stack_alloc(&SUB, "gosub",    STACKSIZE);
  stack_alloc(&ITR, "iterator", STACKSIZE);
  stack_alloc(&IDX, "index",    STACKSIZE);

  js_alloc(&p, stdin, 128);

  if (parse_commands(argc - optind, argv + optind, wordv)) {
    print_stack(OUT);
    buf_write(OUTBUF, '\n');
  }
  stack_pop_to(OUT, -1);
  js_reset(p);

  while ((err = js_parse_one(p, &root)) != JS_EDONE) {
    if (err) die("can't parse JSON");

    // The input buffer now looks something like this:
    //
    //    [XXXXXXYYYY--------]
    //           ^   ^
    //           pp  bp
    //
    // where the Xs are the bytes in the current JSON object just read from
    // stdin, the Ys are more bytes read from stdin but which are not part of
    // the current JSON object and have not yet been parsed. The pp and bp
    // pointers point to the end of the parsed input and the end of bytes read
    // from stdin. The minuses indicate bytes in the input buffer than have
    // been allocated but are not being used at the moment.
    //
    // The + command parses nested JSON (JSON embedded in strings, xzibit style).
    // For this to work we need space in the input buffer to write out the JSON
    // unescaped contents of those strings. We use the end of the input buffer
    // for this purpose, by moving pp to coincide with bp:
    //
    //    [XXXXXXYYYY--------]
    //               ^
    //               pp
    //               bp
    //
    // Now if the + command needs to parse some JSON it writes the unescaped
    // string contents to the input buffer:
    //
    //    [XXXXXXYYYYZZZZZ---]
    //               ^    ^
    //               pp   bp
    //
    // And then parses it:
    //
    //    [XXXXXXYYYYZZZZZ---]
    //                    ^
    //                    bp
    //                    pp
    //
    // Additionally, we set the parser's input stream to /dev/null to prevent
    // it from reading any bytes from stdin into the scratch space at the end
    // of the input buffer that we're using for the nested JSON. A situation
    // like this would be difficult to manage:
    //
    //    [XXXXXXYYYYZZZZZYYYZZZZYY----]
    //
    // When all the commands have finished executing for the current JSON
    // object (i.e. the XXXXXX bytes) we must restore the input buffer pointers
    // to their original states:
    //
    //    [XXXXXXYYYYZZZZZ---]
    //           ^   ^
    //           pp  bp
    //
    // This way we continue parsing from where we left off, and the Zs get
    // overwritten by bytes read from stdin (more Ys).

    bpos = (p->js)->pos;
    ppos = p->pos;
    p->pos = bpos;
    p->in = devnull;

    stack_push(IDX, js_create_index(p, idx++));
    stack_push(DAT, root);

    do {
      if (run(wordc, wordv) > 0) {
        print_stack(OUT);
        buf_println(OUTBUF);
        buf_reset(OUTBUF, 0);
      }

      stack_pop_to(DAT, 0);
      stack_pop_to(OUT, -1);
      stack_pop_to(SUB, -1);
    } while (stack_depth(ITR) > 0);

    // Restore parser to the saved state.
    (p->js)->pos = bpos;
    p->pos = ppos;
    p->in = stdin;

    js_reset(p);
    stack_pop_to(DAT, -1);
    stack_pop_to(IDX, -1);
  }

#ifdef JT_VALGRIND
  js_free(&p);

  buf_free(&OUTBUF);

  stack_free(&DAT);
  stack_free(&OUT);
  stack_free(&SUB);
  stack_free(&ITR);
  stack_free(&IDX);

  free(wordv);
  fclose(devnull);
#endif /* JT_VALGRIND */

  return 0;
}
