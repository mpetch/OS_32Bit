#ifndef _PCB_H
#define _PCB_H

#include <stdint.h>
#include <vga.h>

#define MAX_PIDs 4294967295U
#define STACK_SIZE 4096

enum State {
  READY = 0,
  RUNNING = 1,
  WAITING = 2,
  TERMINATED = 3
};

typedef struct task {
    uint32_t pid;               // Process ID
    enum State state;           // Process state
    uintptr_t pc;               // Program Counter
    uintptr_t sp;               // Stack Pointer
    uint32_t registers[8];      // General-purpose registers
    uint32_t flags;             // Status/Flags register
    struct task *next;          // Pointer to the next task in the list
} task;

extern task* current;

task* create_task(uintptr_t task_entry_function);
void remove_task(struct task* task_terminate);
void print_task_and_count();
uint32_t new_pid();
void free_task_resources(task* terminated_task);
void set_task_state(task* task, enum State state);

#endif