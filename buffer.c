/*
 * buffers
 *****************************************************************************/

#include "buffer.h"
#include "util.h"

void buf_append(Buffer *b, const char *s, size_t len) {
  while (b->size <= b->pos + len + 1)
    b->buf = jrealloc(b->buf, (b->size *= 2.5));
  strncpy(b->buf + b->pos, s, len);
  (b->buf)[b->pos += len] = '\0';
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
