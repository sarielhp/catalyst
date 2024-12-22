/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
 * cancel.C -
 *     
\*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

#include  <stdlib.h>
#include  <stdio.h>
#include  <assert.h>
#include <pthread.h>


/*--- Constants ---*/

void *thread_runner(void* arg)
{   
    //catch the pthread_object as argument
    pthread_t obj = *((pthread_t*)arg);

    //ENABLING THE CANCEL FUNCTIONALITY
    int prevType;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &prevType);

    int i=0;
    for( ; i < 11 ; i++)//1 - > 10
    {
        if(i == 5)
            pthread_cancel(obj);
        else
            printf("count -- %d\n\n", i);
    }
    printf("done");

    return  NULL;
}

int main(int argc, char *argv[])
{
    pthread_t obj;

    pthread_create(&obj, NULL, thread_runner, (void*)&obj);

    pthread_join(obj, NULL);

    return 0;
}


/* cancel.C - End of File ------------------------------------------*/
