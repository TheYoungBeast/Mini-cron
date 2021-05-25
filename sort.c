#include <stdlib.h>
#include <stdio.h>

#include "sort.h"
#include "taskstruct.h"

int comparator(const void* elem1, const void* elem2)
{
    task* t1 = (task*) elem1;
    task* t2 = (task*) elem2;
    
    if(t1->hr >= t2->hr)
    {
        if(t1->min >= t2->min)
            return 1;
        else
            return -1;
    }
    
    if(t1->hr < t2->hr)
        return -1;

    return 0;
}

/**
 * Sorts tasks
 * @param mngr pointer to task_manager (struct)
 */
void sort_task(task_manager* mngr)
{
    qsort(mngr->array.data, mngr->array.size, sizeof(task), comparator);
}