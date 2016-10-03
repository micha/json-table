/*
 * stacks
 *****************************************************************************/

#include "stack.h"
#include "util.h"

void stack_push(Stack *stack, size_t item) {
  if (stack->head >= stack->size)
    die("%s stack overflow", stack->name);
  (stack->items)[++(stack->head)] = item;
}

void stack_pop(Stack *stack) {
  if (--(stack->head) < -1)
    die("%s stack underflow", stack->name);
}

void stack_pop_to(Stack *stack, int n) {
  if (n < -1 || n >= stack->size)
    die("%s stack index out of range: %d", stack->name, n);
  stack->head = n;
}

size_t stack_head(Stack *stack) {
  return stack->head < 0 ? 0 : (stack->items)[stack->head];
}

size_t stack_depth(Stack *stack) {
  return (size_t) stack->head + 1;
}

void stack_alloc(Stack **s, const char *name, int size) {
  *s = jmalloc(sizeof(Stack));
  (*s)->items = jmalloc(sizeof(size_t) * size);
  (*s)->size  = size;
  (*s)->name  = name;
  (*s)->head  = -1;
}

void stack_free(Stack **s) {
  free((*s)->items);
  free(*s);
  *s = NULL;
}
