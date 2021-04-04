#ifndef TASK_STRUCT_H
#define TASK_STRUCT_H

typedef enum task_mode
{
    MODE_STDOUT,
    MODE_STDERR,
    MODE_BOTH
} task_mode;

typedef struct task
{
    int hr;
    int min;
    char command[ 256 ];
    task_mode mode; 
} task;

typedef struct task_array
{
    unsigned size;
    task* data;
} task_array;

#endif