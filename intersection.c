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

/*
 * Intersection mutex for the basic solution
 */
pthread_mutex_t basic_mutex = PTHREAD_MUTEX_INITIALIZER;
// static bool isTerm[4][4];

/*
 * supply_arrivals()
 *
 * A function for supplying arrivals to the intersection
 * This should be executed by a separate thread
 */
static void* supply_arrivals()
{
  int t = 0;
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

typedef struct {
  Side side;
  Direction dir;
} LightArgs;

static void* manage_light(void* arg)
{
  // TODO:
  // while not all arrivals have been handled, repeatedly:
  //  - wait for an arrival using the semaphore for this traffic light
  //  - lock the right mutex(es)
  //  - make the traffic light turn green
  //  - sleep for CROSS_TIME seconds
  //  - make the traffic light turn red
  //  - unlock the right mutex(es)
  LightArgs* lightArgs = (LightArgs*)arg;
  Side side = lightArgs->side;
  Direction dir = lightArgs->dir;

  free(arg);

  int next_car = 0;

  while (true) {
    if (sem_wait(&semaphores[side][dir]) == -1 && errno == EINTR) {
      continue;
    }

    if (get_time_passed() >= END_TIME || next_car == 20) {
      break;
    }

    Arrival car = curr_arrivals[side][dir][next_car];

    if (car.direction == -1) {
      break;
    }

    next_car++;

    pthread_mutex_lock(&basic_mutex);

    int t_green = get_time_passed();
    printf("traffic light %d %d turns green at time %d for car %d\n", side, dir, t_green, car.id);

    sleep(CROSS_TIME);

    int t_red = get_time_passed();
    printf("traffic light %d %d turns red at time %d for car %d\n", side, dir, t_red, car.id);

    pthread_mutex_unlock(&basic_mutex);
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

  // start the timer
  start_time();
  
  // TODO: create a thread per traffic light that executes manage_light
  pthread_t traffic_lights[4][4];

  for (int side = NORTH; side <= WEST; side++) {
    for (int dir = LEFT; dir <= UTURN; dir++) {
      if (side == NORTH) {
        continue;
      }

      if (dir == UTURN && side != SOUTH) {
        continue;
      }

      LightArgs* args = malloc(sizeof(LightArgs));
      args->side = side;
      args->dir = dir;

      if (pthread_create(&traffic_lights[side][dir], 
                        NULL, 
                        manage_light, 
                        args) != 0) {
                          fprintf(stderr, "Error creating thread for side %d, dir %d\n", side, dir);
                          free(args);
                        }
    }
  }

  // TODO: create a thread that executes supply_arrivals
  pthread_t supply_thread;
  pthread_create(&supply_thread, NULL, supply_arrivals, NULL);

  // TODO: wait for all threads to finish
  pthread_join(supply_thread, NULL);

  // I guess just in case we havent hit end time we should right
  while (get_time_passed() <= END_TIME) {
    usleep(10000);
  }

  for (int side = NORTH; side <= WEST; side++) {
    for (int dir = LEFT; dir <= UTURN; dir++) {
      if (side == NORTH) {
        continue;
      }

      if (dir == UTURN && side != SOUTH) {
        continue;
      }

      sem_post(&semaphores[side][dir]);
      if (pthread_join(traffic_lights[side][dir], NULL) != 0) {
        fprintf(stderr, "Error creating thread for side %d, dir %d\n", side, dir);
      }
    }
  }

  // destroy semaphores
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      sem_destroy(&semaphores[i][j]);
    }
  }

  pthread_mutex_destroy(&basic_mutex);
}
