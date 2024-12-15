#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include "arrivals.h"
#include "intersection_time.h"
#include "input.h"

#define NUM_SIDE 4
#define NUM_DIRECTION 4
#define NUM_LIGHTS 10

/* 
 * curr_arrivals[][][]
 *
 * A 3D array that stores the arrivals that have occurred
 * The first two indices determine the entry lane: first index is Side, second index is Direction
 * curr_arrivals[s][d] returns an array of all arrivals for the entry lane on side s for direction d,
 *   ordered in the same order as they arrived
 */
static Arrival curr_arrivals[4][4][20];

/*
 * semaphores[][]
 *
 * A 2D array that defines a semaphore for each entry lane,
 *   which are used to signal the corresponding traffic light that a car has arrived
 * The two indices determine the entry lane: first index is Side, second index is Direction
 */
static sem_t semaphores[4][4];

typedef struct
{
  int side;
  int direction;
  int index_mutex_this;
  int num_involved_mutex;
  int* indexes_involved_mutex;
}Lane;

const Lane lanes[NUM_LIGHTS] = {
  {1,0,0,6,(int[]){0,3,4,6,8,9}},
  {1,1,1,4,(int[]){1,3,4,7}},
  {1,2,2,3,(int[]){2,4,7}},
  {2,0,3,5,(int[]){0,1,3,7,8}},
  {2,1,4,6,(int[]){0,1,2,4,7,8}},
  {2,2,5,2,(int[]){5,8}},
  {2,3,6,3,(int[]){0,6,9}},
  {3,0,7,5,(int[]){1,2,3,4,7}},
  {3,1,8,5,(int[]){0,3,4,5,8}},
  {3,2,9,3,(int[]){0,6,9}}
};

static pthread_mutex_t mutexes_lanes[NUM_LIGHTS];

static void initial_mutexex() {
  for (int i = 0; i < NUM_LIGHTS; i++)
  {
    pthread_mutex_init(&mutexes_lanes[i], NULL);
  }  
}

/*
 * supply_arrivals()
 *
 * A function for supplying arrivals to the intersection
 * This should be executed by a separate thread
 */
static void* supply_arrivals()
{
  int t = 0;
  // num_curr_arrivals[side][direction]
  int num_curr_arrivals[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};

  // for every arrival in the list
  for (int i = 0; i < sizeof(input_arrivals)/sizeof(Arrival); i++)
  {
    // get the next arrival in the list
    Arrival arrival = input_arrivals[i];
    // wait until this arrival is supposed to arrive
    sleep(arrival.time - t);
    t = arrival.time;
    // store the new arrival in curr_arrivals
    curr_arrivals[arrival.side][arrival.direction][num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
    num_curr_arrivals[arrival.side][arrival.direction] += 1;
    // increment the semaphore for the traffic light that the arrival is for
    sem_post(&semaphores[arrival.side][arrival.direction]);
  }

  return(0);
}


/*
 * manage_light(void* arg)
 *
 * A function that implements the behaviour of a traffic light
 */
static void* manage_light(void* arg)
{
  // TODO:
  // while NOT all arrivals have been handled, repeatedly:
  //  - wait for an arrival using the semaphore for this traffic light
  //  - lock the right mutex(es)
  //  - make the traffic light turn green
  //  - sleep for CROSS_TIME seconds
  //  - make the traffic light turn red
  //  - unlock the right mutex(es)
  int next_car = 0;  
  int *argi = (int*)arg;
  int index_lane = *argi;
  free(arg);
  Lane lane_info = lanes[index_lane];
  int side = lane_info.side;
  int direction = lane_info.direction;
  int index_itself = lane_info.index_mutex_this;
  int num_mutexes = lane_info.num_involved_mutex;
  int* indexes_mutex_involved = lane_info.indexes_involved_mutex;

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  int elapsed_time = get_time_passed();
  ts.tv_sec += (END_TIME - elapsed_time);

  while(elapsed_time <= END_TIME) {
    int result = sem_timedwait(&semaphores[side][direction], &ts);
    if (result == -1 && errno == ETIMEDOUT) {      
      break;
    }
        
    // checking phase
    for (int i = 0; i < num_mutexes; i++)
    {
      int index_mutex = indexes_mutex_involved[i];
      int result = pthread_mutex_trylock(&mutexes_lanes[index_mutex]);
      if (result == 0) {
        // lock succeed
      } else if (result == EBUSY) {
        pthread_mutex_lock(&mutexes_lanes[index_mutex]);  //keep waiting
      } else {
        perror("Error checking mutex state");
      }
    }

    // release other mutexes
    for (int i = 0; i < num_mutexes; i++)
    {
      int index_mutex = indexes_mutex_involved[i];
      if (index_itself != index_mutex) {
        pthread_mutex_unlock(&mutexes_lanes[index_mutex]);
      }
    }
        
    Arrival car = curr_arrivals[side][direction][next_car];
    elapsed_time = get_time_passed();
    printf("traffic light %d %d turns green at time %d for car %d\n", side, direction, elapsed_time, car.id);
    sleep(CROSS_TIME);

    elapsed_time = get_time_passed();
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (END_TIME - elapsed_time); //update the time countdown
    printf("traffic light %d %d turns red at time %d\n", side, direction, elapsed_time);
    next_car++;
    if (next_car >= 20) {
      break;
    }

    pthread_mutex_unlock(&mutexes_lanes[index_itself]);        
  }
  return(0);
}


int main(int argc, char * argv[])
{
  // create semaphores to wait/signal for arrivals
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      sem_init(&semaphores[i][j], 0, 0);
    }
  }
  initial_mutexex();

  // start the timer
  start_time();
  
  // TODO: create a thread per traffic light that executes manage_light  
  pthread_t traffic_lights[NUM_LIGHTS];

  for (int i = 0; i < NUM_LIGHTS; i++)
  {
    int *idx = malloc(sizeof(int));
    *idx = i;
    pthread_create(&traffic_lights[i], NULL, manage_light, idx);
  }     

  // TODO: create a thread that executes supply_arrivals
  pthread_t arrival_t;
  pthread_create(&arrival_t, NULL, supply_arrivals, NULL);

  // TODO: wait for all threads to finish
  pthread_join(arrival_t, NULL);  

  for (int i = 0; i < NUM_LIGHTS; i++)
  {
    pthread_join(traffic_lights[i], NULL);
  }
    
  // destroy semaphores
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      sem_destroy(&semaphores[i][j]);
    }
  }

  return 0;
}