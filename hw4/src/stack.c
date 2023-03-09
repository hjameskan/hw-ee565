#include <string.h>
#include <stdio.h>
#include "stack.h"
// // Define the structure for a node in the stack
// typedef struct StackNode {
//     void *data;
//     struct StackNode *next;
// } StackNode;

// // Define the structure for the stack itself
// typedef struct Stack {
//     int size;
//     StackNode *top;
// } Stack;

// Create a new node for the stack with the specified data
static StackNode *create_node(void *data) {
    // printf("data: %s \n", (char*)data);
    fflush(stdout);
    StackNode *node = (StackNode*)malloc(sizeof(StackNode));
    // void *data_copy = malloc(sizeof(data));
    // memcpy(data_copy, data, sizeof(data));
    // node->data = data_copy;
    node->data = data;
    node->next = NULL;
    return node;
}

static StackNode *create_node_str(void *data) {
    StackNode *node = (StackNode*)malloc(sizeof(StackNode));
    void *data_copy = malloc(sizeof(data));
    memcpy(data_copy, data, sizeof(data));
    node->data = data_copy;
    // node->data = data;
    node->next = NULL;
    return node;
}

// Initialize a new stack
Stack *stack_create() {
    Stack *stack = (Stack*)malloc(sizeof(Stack));
    stack->size = 0;
    stack->top = NULL;
    return stack;
}

// Check if the stack is empty
int stack_is_empty(Stack *stack) {
    return stack->size == 0;
}

// Push an element onto the top of the stack
void stack_push(Stack *stack, void *data) {
    StackNode *node = create_node(data);
    node->next = stack->top;
    stack->top = node;
    stack->size++;
}

void stack_push_str(Stack *stack, void *data) {
    StackNode *node = create_node(data);
    node->next = stack->top;
    stack->top = node;
    stack->size++;
}

// Pop an element from the top of the stack and return its data
void *stack_pop(Stack *stack) {
    if (stack_is_empty(stack)) {
        return NULL;
    }
    StackNode *node = stack->top;
    stack->top = node->next;
    stack->size--;
    void *data = node->data;
    free(node);
    return data;
}

// Get the size of the stack
int stack_size(Stack *stack) {
    return stack->size;
}

// Destroy the stack and free all its nodes
void stack_destroy(Stack *stack) {
    while (!stack_is_empty(stack)) {
        stack_pop(stack);
    }
    free(stack);
}
