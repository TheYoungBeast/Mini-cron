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
#include "misc.h"

#define APP_USAGE_INFO          "Usage: minicron <taskfile> <outfile>\n"
#define TASK_PROCESSING_INFO    "Processing the task: [%s]"
#define TASK_COMPLETE_MESSAGE   "Task: [%02d:%02d] %s has been executed with following output: "
#define PROCESS_RETURNED        "Process %s returned with status code: %d"

/**
 * 
 * You don’t have to use openlog. If you call syslog without having called openlog,
 * syslog just opens the connection implicitly and uses defaults for the information
 * in ident and options.
 * 
 **/

volatile task_manager task_mngr = (task_manager){ 0 };
volatile char* taskfile = NULL;

/**
 * Prepears array of arguments for execvp function
 * @param tcommand command line
 * @return An array of arguments
 */
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
        handle_fork_error();

    if (pid > 0) // exit parent process
        exit(EXIT_SUCCESS);

    umask(0); // sets the calling process's file mode creation mask
                
    pid_t sid = setsid(); // become new process group leader
    if (sid < 0)
        handle_setsid_error();

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
        
        if(sleeptime > timeinfo->tm_sec)
            sleeptime -= timeinfo->tm_sec; // make it "to second" precision
        
        if(sleep(sleeptime)) // sleep interupted
            continue; 

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
                    handle_fork_error();

                if(pid == (pid_t) 0) // child 1
                {
                    pid_t sid = setsid();

                    if(sid < 0)
                        handle_setsid_error();

                    int pipedes[ 2 ];
                    if( pipe(pipedes) < 0 ) // pipe & error handling
                        handle_pipe_error(pipedes);

                    pid2 = fork();
                    if (pid2 < 0) // fork failure
                        handle_fork_error();

                    if(pid2 == (pid_t) 0) // child2
                    {
                        close(pipedes[ 1 ]); // close write-end

                        int flags = O_WRONLY | O_APPEND | O_CREAT;
                        mode_t mode = S_IRWXU;
                        int output = open(argv[ 2 ], flags, mode);

                        if(output < 0) // open file failure
                             handle_open_error(argv[ 2 ], flags, mode);

                        dup2(pipedes[ 0 ], STDIN_FILENO);

                        switch(pending_task->mode)
                        {
                            case 0:
                            {
                                dup2(output, STDERR_FILENO);
                                break;
                            }
                            case 1:
                            {
                                dup2(output, STDOUT_FILENO);
                                break;
                            }
                            case 2:
                            {
                                dup2(output, STDERR_FILENO);
                                dup2(output, STDOUT_FILENO);
                                break;
                            }
                        }

                        /* pre message/header before output dump */
                        char pretask_message[ 1024 ] = {'\0'};
                        sprintf(pretask_message, TASK_COMPLETE_MESSAGE, pending_task->hr, pending_task->min, pending_task->command);
                        write(STDOUT_FILENO, pretask_message, strlen(pretask_message));

                        if (execvp(args2[ 0 ], args2) < 0) // exec failure
                            handle_execvp_error(args2[ 0 ], args2);

                    } // child 2
                    else // child 1
                    { 
                        close(pipedes[ 0 ]); // close read-end
                        dup2(pipedes[ 1 ], STDOUT_FILENO);

                        if (execvp(args1[ 0 ], args1) < 0) // exec failure
                            handle_execvp_error(args1[ 0 ], args1);
                    }
                } // child 1
                else // parent
                {
                    int status1, status2;
                    waitpid(pid, &status1, 0);
                    waitpid(pid2, &status2, 0);

                    syslog(LOG_INFO, PROCESS_RETURNED, command1, status1);
                    syslog(LOG_INFO, PROCESS_RETURNED, command2, status2);
                }
            }
            else // single task (non-pipable)
            {
                char** args = get_command_args(pending_task->command);

                pid_t pid = fork();
                if (pid < 0) // fork failure
                    handle_fork_error();

                if(pid == (pid_t) 0) // child
                {
                    pid_t sid = setsid();

                    if(sid < 0)
                        handle_setsid_error();

                    int flags = O_WRONLY | O_APPEND | O_CREAT;
                    mode_t mode = S_IRWXU;
                    int output = open(argv[ 2 ], flags, mode); // open output file

                    if(output < 0) // open file failure
                        handle_open_error(argv[ 2 ], flags, mode);

                    switch(pending_task->mode)
                    {
                        case 0:
                        {
                            dup2(output, STDERR_FILENO);
                            break;
                        }
                        case 1:
                        {
                            dup2(output, STDOUT_FILENO);
                            break;
                        }
                        case 2:
                        {
                            dup2(output, STDERR_FILENO);
                            dup2(output, STDOUT_FILENO);
                            break;
                        }
                    }

                    /* pre message/header before output dump */
                    char pretask_message[ 1024 ] = {'\0'};
                    sprintf(pretask_message, TASK_COMPLETE_MESSAGE, pending_task->hr, pending_task->min, pending_task->command);
                    write(STDOUT_FILENO, pretask_message, strlen(pretask_message));

                    if (execvp(args[ 0 ], args) < 0) // exec failure
                        handle_execvp_error(args[ 0 ], args);
                }
                else // parent
                {
                    int status;
                    waitpid(pid, &status, 0);
                    syslog(LOG_INFO, PROCESS_RETURNED, pending_task->command, status);
                }
            }

            /* Close out the standard file descriptors */
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            /* change task status */ 
            pending_task->status = STATUS_DONE;
        }

        task_mngr.queue_size = 0;
        task_mngr.queue = realloc(task_mngr.queue, 0);
    }

    return 0;
}