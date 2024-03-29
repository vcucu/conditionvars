/*
 * Operating Systems  [2INCO]  Practical Assignment
 * Condition Variables Application
 *
 * Veronika Cucorova (1013687)
 * Diana Epureanu (0992861)
 *
 * Grading:
 * Students who hand in clean code that fully satisfies the minimum requirements will get an 8.
 * "Extra" steps can lead to higher marks because we want students to take the initiative.
 * Extra steps can be, for example, in the form of measurements added to your code, a formal
 * analysis of deadlock freeness etc.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "prodcons.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static ITEM buffer[BUFFER_SIZE];

static void rsleep (int t);			// already implemented (see below)
static ITEM get_next_item (void);	// already implemented (see below)
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t conditionProd[NROF_PRODUCERS] = {PTHREAD_COND_INITIALIZER};
static pthread_cond_t conditionCons = PTHREAD_COND_INITIALIZER;
pthread_t   t_list[NROF_PRODUCERS+2];
int huge[NROF_ITEMS+1] = {-1};
int next = 0;
int count = 0;

typedef struct par{
    int index;
} param;


void print_buffer()
{
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        printf("%d |",buffer[i]);
    }
    printf("\n");
    printf("\n");

}

void initialize_buffer()
{
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        buffer[i] = -1;
    }
}

void put(ITEM item)
{
    //printf("     P:inserting item %d into buffer\n", item);
    buffer[(item%BUFFER_SIZE)] = item;
    count++;
    //print_buffer();
}

ITEM get()
{
    ITEM item = buffer[next%BUFFER_SIZE];
    //printf("          C: reading item %d from position %d\n", item, next%BUFFER_SIZE);
    buffer[next%BUFFER_SIZE] = -1;
    next++;
    count--;

    //print_buffer();
    return item;
}

/* producer thread */
static void *
producer (void * arg)
{
    ITEM item = get_next_item();
    int position = ((param *) arg)-> index;
    free (arg);

    while (item < NROF_ITEMS)
    {
        // * get the new item

                rsleep (100);
        // mutex-lock;
				pthread_mutex_lock (&mutex);
				//printf("     P:locking %d\n", item);

        //while not condition-for-this-producer
        //wait-cv;
                huge[item] = position;
				while(next <= item - BUFFER_SIZE)
				{

                        //printf("     P:waiting for condition %d\n", item);
						pthread_cond_wait (&conditionProd[position], &mutex);
				}

				put(item);
                //possible-cv-signals;
                //printf("     P:signaling to consumer %d\n", item);
                pthread_cond_signal (&conditionCons);

				//mutex-unlock;
				pthread_mutex_unlock (&mutex);
				item = get_next_item();
    }
	return (NULL);
}

/* consumer thread */
static void *consumer (void * arg)
{
    while (next != NROF_ITEMS)
    {
        //mutex-lock;
                //printf("          C: locking\n");
				pthread_mutex_lock (&mutex);
        //while not condition-for-this-consumer
				while ((buffer[next%BUFFER_SIZE] == -1))
				{
		      //wait-cv;
                    //printf("          C: waiting for condition\n");
					pthread_cond_wait(&conditionCons, &mutex);
				}
        //critical-section;
                ITEM item = get();
                printf("%d\n", item); //THE ONLY PRINT THAT SHOULD STAY IN SUBMISSION
                //printf("Next: %d\n", next);

        //possible-cv-signals;
                int bound  = MIN(next + BUFFER_SIZE -1, NROF_ITEMS-1);
                for (int i = next; i <= bound; i++)
                {
                    //printf("Signal to thread %d\n", huge[i]);
                    pthread_cond_signal(&conditionProd[huge[i]]);
                }

        //mutex-unlock;
				pthread_mutex_unlock(&mutex);

        rsleep (100);
    }
	return (NULL);
}

int main (void)
{
    // TODO:
    // * startup the producer threads and the consumer thread
    //pthread_t consumerThread;
    //pthread_t producerThread;
    initialize_buffer();
    for (int i = 0; i < NROF_PRODUCERS; i++)
    {

        param *  parameter;
        parameter = malloc(sizeof(param));
        parameter->index = i;

        pthread_create (&t_list[i], NULL, producer, parameter);
        //printf("Creating producer %d\n", i);
    }
    pthread_create (&t_list[NROF_PRODUCERS], NULL, consumer, NULL);
    //printf("Creating consumer %d\n", NROF_PRODUCERS);



    // * wait until all threads are finished
    for (int i = 0; i < NROF_PRODUCERS+1 ; i ++)
    {
        pthread_join(t_list[i], NULL);
    }

    for (int i = 0; i < NROF_ITEMS; i++)
    {
      //  printf("%d; ", huge[i]);
    }


    return (0);
}

/*
 * rsleep(int t)
 *
 * The calling thread will be suspended for a random amount of time between 0 and t microseconds
 * At the first call, the random generator is seeded with the current time
 */
static void
rsleep (int t)
{
    static bool first_call = true;

    if (first_call == true)
    {
        srandom (time(NULL));
        first_call = false;
    }
    usleep (random () % t);
}


/*
 * get_next_item()
 *
 * description:
 *		thread-safe function to get a next job to be executed
 *		subsequent calls of get_next_item() yields the values 0..NROF_ITEMS-1
 *		in arbitrary order
 *		return value NROF_ITEMS indicates that all jobs have already been given
 *
 * parameters:
 *		none
 *
 * return value:
 *		0..NROF_ITEMS-1: job number to be executed
 *		NROF_ITEMS:		 ready
 */
static ITEM
get_next_item(void)
{
    static pthread_mutex_t	job_mutex	= PTHREAD_MUTEX_INITIALIZER;
	static bool 			jobs[NROF_ITEMS+1] = { false };	// keep track of issued jobs
	static int              counter = 0;    // seq.nr. of job to be handled
    ITEM 					found;          // item to be returned

	/* avoid deadlock: when all producers are busy but none has the next expected item for the consumer
	 * so requirement for get_next_item: when giving the (i+n)'th item, make sure that item (i) is going to be handled (with n=nrof-producers)
	 */
	pthread_mutex_lock (&job_mutex);

    counter++;
	if (counter > NROF_ITEMS)
	{
	    // we're ready
	    found = NROF_ITEMS;
	}
	else
	{
	    if (counter < NROF_PRODUCERS)
	    {
	        // for the first n-1 items: any job can be given
	        // e.g. "random() % NROF_ITEMS", but here we bias the lower items
	        found = (random() % (2*NROF_PRODUCERS)) % NROF_ITEMS;
	    }
	    else
	    {
	        // deadlock-avoidance: item 'counter - NROF_PRODUCERS' must be given now
	        found = counter - NROF_PRODUCERS;
	        if (jobs[found] == true)
	        {
	            // already handled, find a random one, with a bias for lower items
	            found = (counter + (random() % NROF_PRODUCERS)) % NROF_ITEMS;
	        }
	    }

	    // check if 'found' is really an unhandled item;
	    // if not: find another one
	    if (jobs[found] == true)
	    {
	        // already handled, do linear search for the oldest
	        found = 0;
	        while (jobs[found] == true)
            {
                found++;
            }
	    }
	}
    jobs[found] = true;

	pthread_mutex_unlock (&job_mutex);
	//printf("Im returning %d\n", found);
	return (found);
}
