/*
 * helpers
 *****************************************************************************/

#include "util.h"

inline int is_digit_char(char x) {
  return ('0' <= x && x <= '9');
}

inline int is_hex_char(char x) {
  return (('0' <= x && x <= '9') ||
          ('a' <= x && x <= 'f') ||
          ('A' <= x && x <= 'F'));
}

inline int is_ws_char(char x) {
  return (x == ' ' || x == '\t' || x == '\r' || x == '\n');
}

size_t strtosizet(const char *s) {
  size_t ret = 0;
  for (; *s != '\0'; s++) {
    if (!is_digit_char(*s)) return SIZE_MAX;
    ret *= 10;
    ret += (size_t) (*s - '0');
  }
  return ret;
}

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
