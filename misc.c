#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <libexplain/pipe.h>
#include <libexplain/fork.h>
#include <libexplain/open.h>
#include <libexplain/chdir.h>
#include <libexplain/setsid.h>
#include <libexplain/execvp.h>

#include "misc.h"


// ERROR HANDLING


/**
 * Function wrapper for open error handling
 * @param path path to file
 * @param flags flags that were used to open file
 * @param mode mode that were used
 */
void handle_open_error(const char* path, int flags, mode_t mode)
{
    int err = errno;
    char message[3000];
    explain_message_errno_open(message, sizeof(message), err, path, flags, mode);
    syslog(LOG_INFO, "%s", message);
    exit(EXIT_FAILURE);
}

/**
 * Function wrapper for fork error handling
 */
void handle_fork_error()
{
    int error = errno;
    char message[3000];
    explain_message_errno_fork(message, sizeof(message), error);
    syslog(LOG_INFO, "%s", message);
    closelog();
    exit(EXIT_FAILURE);
}

/**
 * Function wrapper for setsid error handling
 */
void handle_setsid_error()
{
    int err = errno;
    char message[3000];
    explain_message_errno_setsid(message, sizeof(message), err);
    syslog(LOG_INFO, "%s", message);
    closelog();
    exit(EXIT_FAILURE);
}

/**
 * Function wrapper for pipe error handling
 * @param pipedes An array of descriptors returned by pipe function
 */
void handle_pipe_error(int pipedes[ 2 ])
{
    int err = errno;
    char message[3000];
    explain_message_errno_pipe(message, sizeof(message), err, pipedes);
    write(STDERR_FILENO, message, strlen(message));
    exit(EXIT_FAILURE);
}

/**
 * Function wrapper for execvp error handling
 * @param pathname pathname or bash command itself
 * @param argv An array of additional arguments to the command
 */
void handle_execvp_error(const char* pathname, char* const* argv)
{
    int err = errno;
    char message[3000];
    explain_message_errno_execvp(message, sizeof(message), err, pathname, argv);
    syslog(LOG_INFO, "%s", message);
    exit(EXIT_FAILURE);
}

/**
 * SETTING TASKS
*/

/**
 * Counts sleeptime to the nearest time from a given time
 * @param tarray Pointer to task_array struct with tasks
 * @param current_hr time(hour)
 * @param current_min time(min)
 * @return The absolute number of seconds to sleep
 */
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

/**
 * Sets up queue with tasks by given time
 * @param tm Pointer to task_manager struct
 * @param tasktime task's time in seconds
 */
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

/**
 * Determine whether the given command is supposed to be split into pipes
 * @param command command line
 */
int is_pipable(const char* command)
{
    char* pos = strchr(command, (int) '|');

    if(pos == NULL)
        return 0;
    
    return pos-command;
}