#include <stdio.h>

#include "fileparse.h"
#include "taskstruct.h"

int main(int argc, char* argv[])
{
    if(argc < 3)
    {
        printf("Usage: minicron <taskfile> <outfile>\n");
        return 0;
    }

    task_array array = parse_taskfile(argv[1]);

    for(size_t i = 0; i < array.size; i++)
        printf("%u:%u:%s:%u\n", array.data[i].hr, array.data[i].min, array.data[i].command, array.data[i].mode);

    return 0;
}