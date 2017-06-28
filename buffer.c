/*
 * buffers
 *****************************************************************************/

#include "buffer.h"
#include "util.h"

void buf_print(Buffer *b) {
  printf("%s", b->buf);
}

void buf_println(Buffer *b) {
  printf("%s\n", b->buf);
}

inline void buf_check(Buffer *b, const size_t len) {
  while (b->size <= b->pos + len + 1)
    b->buf = jrealloc(b->buf, (b->size *= 2.5));
}

inline void buf_write_unchecked(Buffer *b, const char c) {
  (b->buf)[b->pos] = c;
  (b->buf)[b->pos += 1] = '\0';
}

inline void buf_append_unchecked(Buffer *b, const char *s, const size_t len) {
  strncpy(b->buf + b->pos, s, len);
  (b->buf)[b->pos += len] = '\0';
}

void buf_write(Buffer *b, const char c) {
  buf_check(b, 1);
  buf_write_unchecked(b, c);
}

void buf_append(Buffer *b, const char *s, size_t len) {
  buf_check(b, len);
  buf_append_unchecked(b, s, len);
}

void buf_append_csv(Buffer *b, const char *s, size_t len) {
  size_t i;

  if (! strchr(s, '\"')) {
    buf_append(b, s, len);
  } else {
    buf_check(b, len * 2);
    for (i=0; i<len; i++) {
      if (s[i] == '\"') buf_write_unchecked(b, '\"');
      buf_write_unchecked(b, s[i]);
    }
  }
}

ssize_t buf_append_read(Buffer *b, FILE *in) {
  char tmp[BUFSIZ];
  ssize_t bytes_r = 0;
  int fd = fileno(in);

  if (fd == -1) die_err("bad input stream");

  if ((bytes_r = read(fd, tmp, sizeof(tmp))) > 0)
    buf_append(b, (const char*) tmp, bytes_r);

  if (bytes_r < 0) die_err("can't read input");

  return bytes_r;
}

void buf_splice(Buffer *b, size_t start, size_t end) {
  if (end > start) {
    if (b->pos > end)
      memmove(b->buf + start, b->buf + end, b->pos - end);
    b->pos -= (end - start);
  }
}

void buf_reset(Buffer *b, size_t start) {
  if (0 < start && start < b->pos)
    memmove(b->buf, b->buf + start, (b->pos -= start));
  else
    b->pos = 0;

  (b->buf)[b->pos] = '\0';
}

void buf_alloc(Buffer **b) {
  *b = jmalloc(sizeof(Buffer));
  (*b)->buf = jmalloc(BUFSIZ * 2.5);
  (*b)->size = BUFSIZ * 2.5;
  buf_reset(*b, 0);
}

void buf_free(Buffer **b) {
  free((*b)->buf);
  free(*b);
  *b = NULL;
}
