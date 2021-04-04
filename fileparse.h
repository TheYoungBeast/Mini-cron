#ifndef FILE_PARSE_H
#define FILE_PARSE_H

#include "taskstruct.h"

void trim(char*);
task_array parse_taskfile(const char*); 

#endif // FILE_PARSE_H