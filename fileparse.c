#include "fileparse.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <libexplain/open.h>
#include <libexplain/read.h>

void trim(char * s) 
{
    char* p = s;
    int l = strlen(p);

    while(isspace(p[l - 1])) p[--l] = 0;
    while(* p && isspace(* p)) ++p, --l;

    memmove(s, p, l + 1);
}

char* parse_command(const char* buff, size_t* pos1, size_t* pos2)
{
    size_t start = 0;
    size_t end = 0;
    unsigned num = 0;

    for(size_t i = 0; i < strlen(buff); i++)
    {
        if(buff[i] == ':') num++;
        if(num == 2)
        {
            start = i;
            break;
        }
    }

    for(size_t i = strlen(buff)-1; i >= 0; i--)
    {
        if(buff[i] == ':')
        {
            end = i;
            break;
        }
    }

    *pos1 = start; *pos2 = end;
    char* command = (char*) malloc(sizeof(char)*(end-start));
    memcpy(command, &buff[start+1],end-start-1);
    return command;
}

task_array parse_taskfile(const char* filename)
{
    int desc = open(filename, O_RDONLY, S_IRUSR);

    if(desc < 0)
    {
        int err = errno;
        char message[3000];
        explain_message_errno_open(message, sizeof(message), err, filename, O_RDONLY, 0);
        write(STDERR_FILENO, message, strlen(message));
        exit(EXIT_FAILURE);
    }

    task_array array = {0, NULL};

    char buffer[ 1024 ];
    FILE* stream = fdopen(desc, "r");

    while(fgets(buffer, sizeof(buffer), stream) != NULL)
    {
        trim(buffer);
        
        if(buffer[0] == '#')
            continue;

        task ttask;
        size_t pos1, pos2;
        char buff2[ 20 ];
        char* command = parse_command(buffer, &pos1, &pos2);
        strcpy(ttask.command, command);
        free(command);
        memcpy(buff2, buffer, pos1);
        memcpy(&buff2[pos1], &buffer[pos2], pos2-pos1);
        sscanf(buff2, "%u:%u:%u", &ttask.hr, &ttask.min, &ttask.mode);
        trim(ttask.command);

        if(ttask.hr >= 24)
            printf("Invalid hour format\n");
        if(ttask.min >= 60)
            printf("Invalid minute format\n");
        if(!(strlen(ttask.command) > 0))
            printf("Invalid command %s\n", ttask.command);
        if(ttask.mode > 2)
            printf("Invalid mode format %u\n", ttask.mode);
        
        array.data = realloc(array.data, sizeof(task)*(++array.size));
        memcpy(&array.data[array.size-1], &ttask, sizeof(ttask));
    }

    close(desc);
    return array;
}