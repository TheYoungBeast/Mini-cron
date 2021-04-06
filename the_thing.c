/* Things that actually get done */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <libexplain/open.h>
#include <libexplain/read.h>
#include <ctype.h>
#include <time.h>
#include "Daemon.h"

void everything(task_array ta, char outfile_argument[]) /*ta-task array */
{
	time_t t =time(NULL);
	struct tm tm= *localtime(&t);	
	/*
	tm.tm_hour - current hour
	tm.tm_min - current minute
	*/
	/*sleep until midnight if there's no more tasks for the day */
	int sleeptime=86340 - ((tm.tm_hour)*3600+(tm.tm_min*60));
	task_mode mode = MODE_STDOUT;
	int cmd_index=-1; /*a task that does nothing */
	for(int i =0; i< ta.size; i++)
	{
		for(int j = tm.tm_hour; j<24; j++)
		{
			for(int k = tm.tm_min; k<60; k++)
			{
				if(ta.data[i].hr == j && ta.data[i].min == k)
				{
				printf("%d", tm.tm_hour);
				printf("%s", "\n");
				printf("%d", tm.tm_min);
				printf("%s", "\n");
				printf("%d", j);
				printf("%s", "\n");
				printf("%d", k);
				printf("%s", "\n");
				printf("%d", (j*3600)+(k*60));
				printf("%s", "\n");
				printf("%d", (tm.tm_hour)*3600+(tm.tm_min*60));
				printf("%s", "\n");
				sleeptime = ((j*3600)+(k*60)-((tm.tm_hour)*3600+(tm.tm_min*60)));
				 mode=ta.data[i].mode;	
				printf("%d", sleeptime);
				printf("%s", "\n"); 		
				 cmd_index=i;
				 j=24; k=60; i=ta.size;
				}
			}
		}
	}
	/*running the Daemon */
	if(cmd_index == -1)
	{
	abcdefg(sleeptime,mode,"false",outfile_argument,ta);
	}
	else
	{
	abcdefg(sleeptime, mode,ta.data[cmd_index].command, outfile_argument,ta);
	}
}
