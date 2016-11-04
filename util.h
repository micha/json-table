/*
 * helpers
 *****************************************************************************/
#ifndef UTIL_H
#define UTIL_H

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

int is_digit_char(char x);

int is_hex_char(char x);

int is_ws_char(char x);

size_t strtosizet(const char *s);

void warn(const char *fmt, ...);

void die(const char *fmt, ...);

void die_err(const char *fmt, ...);

void die_mem();

void *jmalloc(size_t size);

void *jrealloc(void *ptr, size_t size);

#endif
