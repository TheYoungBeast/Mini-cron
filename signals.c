#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>

#include "signals.h"
#include "taskstruct.h"
#include "fileparse.h"
#include "sort.h"

extern volatile task_manager task_mngr;
extern volatile char* taskfile;

void signal_handler(int signum)
{
    static volatile sig_atomic_t terminate = 0;

    switch (signum)
    {
        case SIGUSR1: // reload file
        {
            // free memory in case it was used before
            free(task_mngr.array.data);
            free(task_mngr.queue);
            task_mngr.queue_size = 0;

            // load the data again
            task_mngr.array = parse_taskfile((const char*) taskfile);
            sort_task((task_manager*)&task_mngr);
            
            syslog(LOG_INFO, "Reloading Tasks (file)");
            closelog();
            break;
        }
        case SIGUSR2: // pending tasks
        {
            int num = 0;
            for(size_t i = task_mngr.array.size; i > 0; i--)
                if(task_mngr.array.data[ i-1 ].status == STATUS_PENDING)
                    num++;

            syslog(LOG_INFO, "Pending tasks %d:", num);
            closelog();

            for(size_t i = task_mngr.array.size; i > 0; i--)
            {
                if(task_mngr.array.data[ i-1 ].status != STATUS_PENDING)
                    continue;

                syslog(LOG_INFO, "%02d:%02d task:%s", task_mngr.array.data[ i-1 ].hr, 
                    task_mngr.array.data[ i-1 ].min, task_mngr.array.data[ i-1 ].command);
                closelog();
            }
            break;
        }
        case SIGINT: // Terminate the Process
        {
            if(terminate)
                raise(signum);
            terminate = 1;

            syslog(LOG_INFO, "Terminating daemon");
            closelog();

            free(task_mngr.array.data);
            free(task_mngr.queue);

            signal(signum, SIG_DFL);
            raise(signum);
            break;
        }
        default:
            break;
    }
}