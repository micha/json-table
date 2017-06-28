/*
 * buffers
 *****************************************************************************/

#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>
#include <stdio.h>
#include "util.h"

typedef struct Buffer {
  char *buf;
  size_t pos;
  size_t size;
} Buffer;

void buf_print(Buffer *b);
void buf_println(Buffer *b);
void buf_write(Buffer *b, const char c);
void buf_check(Buffer *b, const size_t len);
void buf_write_unchecked(Buffer *b, const char c);
void buf_append_unchecked(Buffer *b, const char *s, size_t len);
void buf_append(Buffer *b, const char *s, size_t len);
void buf_append_csv(Buffer *b, const char *s, size_t len);
ssize_t buf_append_read(Buffer *b, FILE *in);
void buf_reset(Buffer *b, size_t start);
void buf_splice(Buffer *b, size_t start, size_t len);
void buf_alloc(Buffer **b);
void buf_free(Buffer **b);

#endif
