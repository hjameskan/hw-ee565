#ifndef STACK_H
#define STACK_H

#include <stdlib.h>

// Define the structure for a node in the stack
typedef struct StackNode {
    void *data;
    struct StackNode *next;
} StackNode;

// Define the structure for the stack itself
typedef struct Stack {
    int size;
    StackNode *top;
} Stack;

// Initialize a new stack
Stack *stack_create();

// Check if the stack is empty
int stack_is_empty(Stack *stack);

// Push an element onto the top of the stack
void stack_push(Stack *stack, void *data);

// Pop an element from the top of the stack and return its data
void *stack_pop(Stack *stack);

// Get the size of the stack
int stack_size(Stack *stack);

// Destroy the stack and free all its nodes
void stack_destroy(Stack *stack);

#endif
