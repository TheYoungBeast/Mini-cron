#include "Daemon.h"
#include "the_thing.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
int abcdefg(int sleeptime, task_mode mode,char _command[256], char outfile_argument[],task_array ta)
{	printf("%d",sleeptime);
	char cmd[]="echo ";
	strcat(cmd,_command); /*merges 2 strings */
	strcat(cmd," > ");
	strcat(cmd, outfile_argument);
	char *command[] = {cmd ,NULL};
/* Our process ID and Session ID */
        pid_t pid, sid;
        
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
                exit(EXIT_FAILURE);
        }
        /* If we got a good PID, then
           we can exit the parent process. */
        if (pid > 0) {
                exit(EXIT_SUCCESS);
        }

        /* Change the file mode mask */
        umask(0);
        
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
                /* Log the failure */
                exit(EXIT_FAILURE);
        }
         /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        pid = getpid();
       /*The big loop */  
       /* the Deamon still doesn't work */    
       while(1)
       {
       	while(1)
       	{
			if(getpid() != pid)
		      	   {
		      	   break;
		      	   }
		      	   else
		      	   {
		      	   fork();            	   
		      	   sleep(sleeptime);
		      	   }
              	  }
         execvp("./",command);       
       }
        everything(ta,outfile_argument);      
       exit(EXIT_SUCCESS);
}
