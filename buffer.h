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

void buf_append(Buffer *b, const char *s, size_t len);
ssize_t buf_append_read(Buffer *b, FILE *in);
void buf_reset(Buffer *b, size_t start);
void buf_alloc(Buffer **b);
void buf_free(Buffer **b);

#endif
