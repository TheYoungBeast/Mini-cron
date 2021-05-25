#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <libexplain/pipe.h>
#include <libexplain/fork.h>
#include <libexplain/open.h>
#include <libexplain/chdir.h>
#include <libexplain/setsid.h>
#include <libexplain/execvp.h>

#include "fileparse.h"
#include "taskstruct.h"
#include "signals.h"
#include "sort.h"

#define APP_USAGE_INFO          "Usage: minicron <taskfile> <outfile>\n"
#define TASK_COMPLETE_INFO      "Task: %s has been done recently"
#define TASK_PROCESSING_INFO    "Processing the task: [%s]"
#define TASK_COMPLETE_MESSAGE   "Task: [%02d:%02d] with provided command:%s has been executed with following output: "

/**
 * 
 * You don’t have to use openlog. If you call syslog without having called openlog,
 * syslog just opens the connection implicitly and uses defaults for the information
 * in ident and options.
 * 
 **/

char** get_command_args(const char* tcommand)
{
    char** args = NULL;
    size_t size = 0;

    char command[ strlen(tcommand)+1 ];
    memcpy(command, tcommand, strlen(tcommand)+1);

    for(char* token = strtok(command, "  "); token != NULL; token = strtok(NULL, "  ")) // the command does contain additional parameters
    {                                                                                   // build an array of args
        args = realloc(args, sizeof(char*)*(++size));
        args[ size-1 ] = malloc(sizeof(char)*strlen(token)+1);
        memcpy(args[ size-1 ], token, strlen(token)+1);
    }

    if(strchr(tcommand, ' ') == NULL) // the command does not contain additional parameters
    {                                // however, it still requires an array of args
        args = malloc(sizeof(char*));
        args[ 0 ] = malloc(sizeof(char) * strlen(tcommand)+1);
        memcpy(args[0], tcommand, strlen(tcommand)+1);
        size++;
    }
    
    args = realloc(args, sizeof(char*)*(++size)); // the array has to end with NULL element
    args[ size-1 ] = malloc(sizeof(char));
    args[ size-1 ] = NULL;

    return args;
}

unsigned long find_nearest_time(task_array* tarray, unsigned current_hr, unsigned current_min)
{
    if(tarray == NULL || tarray->size == 0)
        return 0;

    unsigned current_time = (current_hr*60 + current_min)*60;
    unsigned long first_task_time = ((tarray->data[ 0 ].hr*60 + tarray->data[ 0 ].min)*60);
    long diff = 0;

    if(current_time > first_task_time)
        diff = ((24*60*60)-current_time) + first_task_time; // time to end of the day + actuall task time
    else
        diff = first_task_time - current_time;
    
    for(size_t i = 0; i < tarray->size; i++)
    {
        unsigned task_time = (tarray->data[ i ].hr*60 + tarray->data[ i ].min)*60;

        if(task_time < current_time) // All the data is sorted
            continue;

        if((current_time - task_time) < diff)
        {
            diff = current_time - task_time;
            break; // all data is sorted - it means if we find the first (nearest) time 
        }          // the others will be the same or even further than our current time
    }

    return diff;
}

void set_tasks_queue(task_manager* tm, unsigned takstime)
{
    task_array* array = &(tm->array);

    if(array->data == NULL)
        return;

    for(size_t i = 0; i < array->size; i++)
    {
        unsigned ttime = (array->data[ i ].hr*60 + array->data[ i ].min)*60;

        if(ttime < takstime)
            continue;
        
        if(ttime > takstime) // data is sorted, so we won't find more tasks within 'tasktime'
            break;

        tm->queue = realloc(tm->queue, sizeof(task*)*(++tm->queue_size));
        tm->queue[ tm->queue_size-1 ] = &(array->data[ i ]);
    }
} 

int is_pipable(const char* command)
{
    char* pos = strchr(command, (int) '|');

    if(pos == NULL)
        return 0;
    
    return pos-command;
}


volatile task_manager task_mngr = (task_manager){ 0 };
volatile char* taskfile = NULL;


int main(int argc, char** argv)
{
    if(argc < 3)
    {
        write(STDIN_FILENO, APP_USAGE_INFO, strlen(APP_USAGE_INFO));
        return 0;
    }

    taskfile = argv[1];

    pid_t pid = fork();

    if (pid < 0) // fork failure
    {
        int error = errno;
        char message[3000];
        explain_message_errno_fork(message, sizeof(message), error);
        syslog(LOG_INFO, "%s", message);
	    closelog();
        exit(EXIT_FAILURE);
    }

    if (pid > 0) // exit parent process
        exit(EXIT_SUCCESS);

    umask(0);
                
    pid_t sid = setsid(); // become new process group leader
    if (sid < 0) 
    {
        int err = errno;
        char message[3000];
        explain_message_errno_setsid(message, sizeof(message), err);
        syslog(LOG_INFO, "%s", message);
	    closelog();
        exit(EXIT_FAILURE);
    }

    // Change the current working directory
    char pathname[] = {"/"};
    if ((chdir(pathname)) < 0) 
    {
        int err = errno;
        char message[3000];
        explain_message_errno_chdir(message, sizeof(message), err, pathname);
        syslog(LOG_INFO, "%s", message);
        closelog();
        exit(EXIT_FAILURE);
    }
        
    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* signal handlers */
    if(signal(SIGUSR1, signal_handler) == SIG_IGN)
        signal(SIGUSR1, SIG_IGN);

    if(signal(SIGUSR2, signal_handler) == SIG_IGN)
        signal(SIGUSR2, SIG_IGN);

    if(signal(SIGINT, signal_handler) == SIG_IGN)
        signal(SIGINT, SIG_IGN);

    task_mngr.array = parse_taskfile(argv[1]);
    sort_task((task_manager*)&task_mngr);

    while(1)
    {
        time_t rawtime;
        time (&rawtime);
        struct tm * timeinfo = localtime (&rawtime);

        /* count the time (in seconds) to sleep - wake only to do pending tasks */
        unsigned long sleeptime = find_nearest_time((task_array*) &(task_mngr.array), timeinfo->tm_hour, timeinfo->tm_min);
        sleep(sleeptime);
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        unsigned long tasktime = (timeinfo->tm_hour*60 + timeinfo->tm_min)*60;
        set_tasks_queue((task_manager*) &task_mngr, tasktime);

        for(size_t i = 0; i < task_mngr.queue_size; i++)
        {
            if(task_mngr.queue[ i ]->status == STATUS_DONE)
                continue;

            task* pending_task = task_mngr.queue[ i ];
            int pos = 0;

            syslog(LOG_INFO, TASK_PROCESSING_INFO, pending_task->command);
            closelog();

            if((pos = is_pipable(pending_task->command)))
            {
                // split command into 2 to pipe them
                char* command1 = malloc(sizeof(char) * pos + 1);
                char* command2 = malloc(sizeof(char) * (strlen(pending_task->command) - pos));
                memcpy(command1, pending_task->command, pos + 1); command1[ pos ] = '\0'; // null terminator
                memcpy(command2, &(pending_task->command[ pos+1 ]), strlen(pending_task->command) - pos); // copy with null terminator
                
                trim(command1);
                trim(command2);
                
                char** args1 = get_command_args(command1);
                char** args2 = get_command_args(command2);

                pid_t pid = fork();
                pid_t pid2 = (pid_t) 0;

                if (pid < 0) // fork failure
                {
                    int error = errno;
                    char message[3000];
                    explain_message_errno_fork(message, sizeof(message), error);
                    syslog(LOG_INFO, "%s", message);
                    closelog();
                    exit(EXIT_FAILURE);
                }

                if(pid == (pid_t) 0) // child 1
                {
                    int pipedes[ 2 ];
                    if( pipe(pipedes) < 0 ) // pipe & error handling
                    {
                        int err = errno;
                        char message[3000];
                        explain_message_errno_pipe(message, sizeof(message), err, pipedes);
                        write(STDERR_FILENO, message, strlen(message));
                        exit(EXIT_FAILURE);
                    }

                    pid2 = fork();
                    if (pid2 < 0) // fork failure
                    {
                        int error = errno;
                        char message[3000];
                        explain_message_errno_fork(message, sizeof(message), error);
                        syslog(LOG_INFO, "%s", message);
                        closelog();
                        exit(EXIT_FAILURE);
                    }

                    if(pid2 == (pid_t) 0) // child2
                    {
                        close(pipedes[ 1 ]); // close write-end

                        int flags = O_WRONLY | O_APPEND | O_CREAT;
                        mode_t mode = S_IRWXU;
                        int output = open(argv[ 2 ], flags, mode);

                        if(output < 0) // open file failure
                        {
                            int err = errno;
                            char message[3000];
                            explain_message_errno_open(message, sizeof(message), err, argv[ 2 ], flags, mode);
                            syslog(LOG_INFO, "%s", message);
                            exit(EXIT_FAILURE);
                        }

                        dup2(pipedes[ 0 ], STDIN_FILENO);
                        dup2(output, STDOUT_FILENO);

                        /* pre message/header before output dump */
                        char pretask_message[ 1024 ] = {'\0'};
                        sprintf(pretask_message, TASK_COMPLETE_MESSAGE, pending_task->hr, pending_task->min, pending_task->command);
                        write(STDOUT_FILENO, pretask_message, strlen(pretask_message));

                        if (execvp(args2[ 0 ], args2) < 0) // exec failure
                        {
                            int err = errno;
                            char message[3000];
                            explain_message_errno_execvp(message, sizeof(message), err, args2[ 0 ], args2);
                            syslog(LOG_INFO, "%s", message);
                            exit(EXIT_FAILURE);
                        }
                    } // child 2
                    else // child 1
                    { 
                        close(pipedes[ 0 ]); // close read-end
                        dup2(pipedes[ 1 ], STDOUT_FILENO);

                        if (execvp(args1[ 0 ], args1) < 0) // exec failure
                        {
                            int err = errno;
                            char message[3000];
                            explain_message_errno_execvp(message, sizeof(message), err, args1[ 0 ], args1);
                            syslog(LOG_INFO, "%s", message);
                            exit(EXIT_FAILURE);
                        }
                    }
                } // child 1
                else // parent
                {
                    waitpid(pid, NULL, 0);
                    waitpid(pid2, NULL, 0);
                }
            }
            else // single task (non-pipable)
            {
                char** args = get_command_args(pending_task->command);

                pid_t pid = fork();
                if (pid < 0) // fork failure
                {
                    int error = errno;
                    char message[3000];
                    explain_message_errno_fork(message, sizeof(message), error);
                    syslog(LOG_INFO, "%s", message);
                    closelog();
                    exit(EXIT_FAILURE);
                }

                if(pid == (pid_t) 0) // child
                {
                    int flags = O_WRONLY | O_APPEND | O_CREAT;
                    mode_t mode = S_IRWXU;
                    int output = open(argv[ 2 ], flags, mode); // open output file

                    if(output < 0) // open file failure
                    {
                        int err = errno;
                        char message[3000];
                        explain_message_errno_open(message, sizeof(message), err, argv[ 2 ], flags, mode);
                        syslog(LOG_INFO, "%s", message);
                        exit(EXIT_FAILURE);
                    }

                    dup2(output, STDOUT_FILENO);

                    /* pre message/header before output dump */
                    char pretask_message[ 1024 ] = {'\0'};
                    sprintf(pretask_message, TASK_COMPLETE_MESSAGE, pending_task->hr, pending_task->min, pending_task->command);
                    write(STDOUT_FILENO, pretask_message, strlen(pretask_message));

                    if (execvp(args[ 0 ], args) < 0) // exec failure
                    {
                        int err = errno;
                        char message[3000];
                        explain_message_errno_execvp(message, sizeof(message), err, args[ 0 ], args);
                        syslog(LOG_INFO, "%s", message);
                        exit(EXIT_FAILURE);
                    }
                }
                else // parent
                    waitpid(pid, NULL, 0);
            }

            /* Close out the standard file descriptors */
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            /* change task status and log info */
            pending_task->status = STATUS_DONE;
            syslog(LOG_INFO, TASK_COMPLETE_INFO, pending_task->command);
            closelog();
        }

        task_mngr.queue_size = 0;
        task_mngr.queue = realloc(task_mngr.queue, 0);
        sleep(10);
    }

    return 0;
}