/*
 * stacks
 *****************************************************************************/

#ifndef STACK_H
#define STACK_H

#include <stdlib.h>
#include "util.h"

typedef struct Stack {
  size_t *items;
  int size;
  const char *name;
  int head;
} Stack;

void stack_push(Stack *stack, size_t item);

void stack_pop(Stack *stack);

void stack_pop_to(Stack *stack, int n);

size_t stack_head(Stack *stack);

size_t stack_depth(Stack *stack);

void stack_alloc(Stack **s, const char *name, int size);

void stack_free(Stack **s);

#endif
