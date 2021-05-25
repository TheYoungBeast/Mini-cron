#ifndef MISC_H
#define MISC_H

#include <sys/stat.h>
#include <fcntl.h>

#include "taskstruct.h"

void handle_open_error(const char*, int , mode_t);
void handle_fork_error();
void handle_setsid_error();
void handle_pipe_error(int[ 2 ]);
void handle_execvp_error(const char*, char* const*);

unsigned long find_nearest_time(task_array*, unsigned, unsigned);
void set_tasks_queue(task_manager*, unsigned);
int is_pipable(const char*);

#endif // MISC_H
