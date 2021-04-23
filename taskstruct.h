#ifndef TASK_STRUCT_H
#define TASK_STRUCT_H

#include <stdio.h>

typedef enum task_mode
{
    MODE_STDOUT,
    MODE_STDERR,
    MODE_BOTH
} task_mode;

typedef enum task_status
{
    STATUS_PENDING,
    STATUS_DONE
} task_status;

typedef struct task
{
    int         hr;
    int         min;
    char        command[ 256 ];
    task_mode   mode;
    task_status status;
} task;

typedef struct task_array
{
    size_t      size;
    task*       data;
} task_array;

typedef struct task_manager
{
    task_array      array;
    task**          queue;
    size_t          queue_size;
} task_manager;

#endif