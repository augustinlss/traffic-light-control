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

// static pthread_mutex_t      mutex          = PTHREAD_MUTEX_INITIALIZER;

// typedef struct
// {
//   int block_length;
//   int* block_side;
//   int* block_direction;
// }BlockList;

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

// const BlockList blockLists[NUM_SIDE][NUM_DIRECTION] = {
//   [1][0] = {5, (int[]){2,2,2,3,3} , (int[]){0,1,3,1,2}},
//   [1][1] = {3, (int[]){2,2,3}     , (int[]){0,1,0}},
//   [1][2] = {2, (int[]){2,3}       , (int[]){1,0}},
//   [2][0] = {4, (int[]){1,1,3,3}   , (int[]){0,1,0,1}},
//   [2][1] = {5, (int[]){1,1,1,3,3} , (int[]){0,1,2,0,1}},
//   [2][2] = {1, (int[]){3}         , (int[]){1}},
//   [2][3] = {2, (int[]){1,3}       , (int[]){0,2}},
//   [3][0] = {4, (int[]){1,1,2,2}   , (int[]){1,2,0,1}},
//   [3][1] = {4, (int[]){1,2,2,2}   , (int[]){0,0,1,2}},
//   [3][2] = {2, (int[]){1,2}       , (int[]){0,3}}
// };

static pthread_mutex_t mutexes_lanes[NUM_LIGHTS];

static void initial_mutexex() {
  for (int i = 0; i < NUM_LIGHTS; i++)
  {
    pthread_mutex_init(&mutexes_lanes[i], NULL);
  }  
}


// static pthread_mutex_t mutex_side_direction[NUM_SIDE][NUM_DIRECTION];


// static void initial_mutexex() {
//   for (int side = 0; side < NUM_SIDE; side++) {
//     for (int direction = 0; direction < NUM_DIRECTION; direction++) {
//       pthread_mutex_init(&mutex_side_direction[side][direction], NULL);

//       // todo
//       // printf("mutex(%d, %d):%p\n", side, direction, (void*)&mutex_side_direction[side][direction]);
//       // end
//     }
//   }
// }



// static void sortLockOrder(int side, int direction, BlockList *lockMutexOrder){
//   BlockList blockList = blockLists[side][direction];
//   int original_len = blockList.block_length;
//   lockMutexOrder->block_length = original_len + 1;
//   lockMutexOrder->block_side = malloc(lockMutexOrder->block_length * sizeof(int));
//   lockMutexOrder->block_direction = malloc(lockMutexOrder->block_length * sizeof(int));

//   int original_location = 0;
//   int target_location = 0;

//   for (; original_location < original_len; original_location++, target_location++)
//   {
//     if (side < blockList.block_side[original_location]) {      
//       break;
//     }
//     lockMutexOrder->block_side[target_location] = blockList.block_side[original_location];
//     lockMutexOrder->block_direction[target_location] = blockList.block_direction[original_location];
//   }
//   lockMutexOrder->block_side[target_location] = side;
//   lockMutexOrder->block_direction[target_location] = direction;
//   target_location++;
//   for (; original_location < blockList.block_length; original_location++, target_location++)
//   {    
//     lockMutexOrder->block_side[target_location] = blockList.block_side[original_location];
//     lockMutexOrder->block_direction[target_location] = blockList.block_direction[original_location];
//   }

//   // todo
//   // printf("Thread (%d,%d) contains mutextes index: ", side, direction);
//   // for (int i = 0; i < lockMutexOrder->block_length; i++)
//   // {
//   //   printf("(%d, %d) ", lockMutexOrder->block_side[i], lockMutexOrder->block_direction[i]);
//   // }
//   // printf("\n");
//   // end
// }

// static pthread_mutex_t* getMutexList(int side, int direction) {
//   BlockList lockMutexOrder;
//   sortLockOrder(side, direction, &lockMutexOrder);
//   int length = lockMutexOrder.block_length;

//   pthread_mutex_t* mutexList = malloc(length * sizeof(pthread_mutex_t));
//   for (int i = 0; i < length; i++)
//   {
//     int mutex_side = lockMutexOrder.block_side[i];
//     int mutex_direction = lockMutexOrder.block_direction[i];
//     mutexList[i] = mutex_side_direction[mutex_side][mutex_direction];
//   }
//   free(lockMutexOrder.block_side);
//   free(lockMutexOrder.block_direction); 
//   return mutexList;
// }

// static void getMutexList(int side, int direction, pthread_mutex_t* mutexList) {
//   BlockList lockMutexOrder;
//   sortLockOrder(side, direction, &lockMutexOrder);
//   int length = lockMutexOrder.block_length;

//   // pthread_mutex_t* mutexList = malloc(length * sizeof(pthread_mutex_t));
//   for (int i = 0; i < length; i++)
//   {
//     int mutex_side = lockMutexOrder.block_side[i];
//     int mutex_direction = lockMutexOrder.block_direction[i];
//     mutexList[i] = mutex_side_direction[mutex_side][mutex_direction];
//     // todo
//     // printf("Thread (%d, %d) add mutex(%d, %d) to list: %p\n", side, direction, mutex_side, mutex_direction, (void*)&mutex_side_direction[mutex_side][mutex_direction]);    
//     // end
//   }
//   free(lockMutexOrder.block_side);
//   free(lockMutexOrder.block_direction); 
//   // return mutexList;
// }

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
  // int side = argi[0];
  // int direction = argi[1];
  int index_lane = *argi;
  free(arg);
  Lane lane_info = lanes[index_lane];
  int side = lane_info.side;
  int direction = lane_info.direction;
  int index_itself = lane_info.index_mutex_this;
  int num_mutexes = lane_info.num_involved_mutex;
  int* indexes_mutex_involved = lane_info.indexes_involved_mutex;

  // int num_mutex = blockLists[side][direction].block_length + 1;
  // pthread_mutex_t *list_mutex = malloc(num_mutex * sizeof(pthread_mutex_t));
  // getMutexList(side, direction, list_mutex);
  
  // BlockList blockOrder;
  // sortLockOrder(side, direction, &blockOrder);
  // int num_mutex = blockOrder.block_length;
  // int *lockOrderSide = blockOrder.block_side;
  // int *lockOrderDirection = blockOrder.block_direction;

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  int elapsed_time = get_time_passed();
  ts.tv_sec += (END_TIME - elapsed_time);

  while(elapsed_time <= END_TIME) {
    int result = sem_timedwait(&semaphores[side][direction], &ts);
    // todo
    // int checktime = get_time_passed();
    // printf("Thread (%d, %d) wakes at: %d\n", side, direction, checktime);
    // end
    if (result == -1 && errno == ETIMEDOUT) {
      // todo
      elapsed_time = get_time_passed();
      printf("Thread (%d, %d) terminates at: %d\n", side, direction, elapsed_time);
      // end
      break;
    }
    
    // int locked_mutex_side[num_mutex];
    // int locked_mutex_direction[num_mutex];
    // int locked_mutex_num = 0;
    // for(int i = 0; i < num_mutex; i++) {
    //   // pthread_mutex_lock(&list_mutex[i]);  

    //   int mutex_side = lockOrderSide[i];
    //   int mutex_direction = lockOrderDirection[i];
    //   // todo
    //   checktime = get_time_passed();
    //   printf("Thread (%d, %d) try to lock %dth m(%d, %d) at: %d\n", side, direction, i, mutex_side, mutex_direction, checktime);
    //   int result = pthread_mutex_trylock(&mutex_side_direction[mutex_side][mutex_direction]);
    //   if (result == 0) {          
    //       printf("In (%d, %d), m(%d, %d) is unlocked.\n", side, direction, mutex_side, mutex_direction);
    //       pthread_mutex_unlock(&mutex_side_direction[mutex_side][mutex_direction]);
    //   } else if (result == EBUSY) {          
    //       printf("In (%d, %d), m(%d, %d) is LOCKED.\n", side, direction, mutex_side, mutex_direction);
    //   } else {          
    //       perror("Error checking mutex state");
    //   }
    //   // end
    //   if (mutex_side != side || mutex_direction != direction) {
    //     int result = pthread_mutex_trylock(&mutex_side_direction[mutex_side][mutex_direction]);
    //     if (result == 0) {

    //     }
    //   }
    //   pthread_mutex_lock(&mutex_side_direction[mutex_side][mutex_direction]);

    //   // todo
    //   checktime = get_time_passed();
    //   printf("Thread (%d, %d) locked the %dth mutex at: %d\n", side, direction, i, checktime);
    //   printf("Thread (%d, %d) locked mutex: (%d, %d)\n", side, direction, mutex_side, mutex_direction);
    //   // end 
    // }

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
    
    // pthread_mutex_unlock (&mutex);
    // for(int i = 0; i < num_mutex; i++) {
    //   int mutex_side = lockOrderSide[i];
    //   int mutex_direction = lockOrderDirection[i];
    //   pthread_mutex_unlock(&mutex_side_direction[mutex_side][mutex_direction]);
    // }
  }

  // free(list_mutex);

  // free(blockOrder.block_side);
  // free(blockOrder.block_direction);
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
  // pthread_t traffic_lights[4][4];// todo
  pthread_t traffic_lights[NUM_LIGHTS];

  for (int i = 0; i < NUM_LIGHTS; i++)
  {
    int *idx = malloc(sizeof(int));
    *idx = i;
    pthread_create(&traffic_lights[i], NULL, manage_light, idx);
  }
  
 
  // for (int i = 0; i < NUM_SIDE; i++) {
  //   for (int j = 0; j < NUM_DIRECTION; j++)
  //   {
  //     int* temp = malloc(2 * sizeof(int));
  //     temp[0] = i;
  //     temp[1] = j;
  //     isTerminated[i][j] = false;
  //     pthread_create (&traffic_lights[i][j], NULL, manage_light, temp);
  //   }
  // }

  // for (int i = 0; i < NUM_SIDE; i++) {
  //   for (int j = 0; j < NUM_DIRECTION; j++)
  //   {
  //     if (i == 0) {
  //       continue;
  //     }
  //     if (i == 2 || j != 3) {
  //       int* temp = malloc(2 * sizeof(int));
  //       temp[0] = i;
  //       temp[1] = j;
  //       // isTerminated[i][j] = false;
  //       pthread_create (&traffic_lights[i][j], NULL, manage_light, temp);
  //     }      
  //   }
  // }

  // todo
  // printf("From the intersection, all lights has been created\n");
  // end

  // TODO: create a thread that executes supply_arrivals
  pthread_t arrival_t;
  pthread_create(&arrival_t, NULL, supply_arrivals, NULL);

  // TODO: wait for all threads to finish
  pthread_join(arrival_t, NULL);
  // for (int i = 0; i < NUM_SIDE; i++)
  // {
  //   for (int j = 0; j < NUM_DIRECTION; j++) {
  //     isTerminated[i][j] = true;
  //     sem_post(&semaphores[i][j]);
  //     pthread_join(traffic_lights[i][j], NULL);
  //   }
  // }

  // for (int i = 0; i < NUM_SIDE; i++)
  // {
  //   for (int j = 0; j < NUM_DIRECTION; j++) {
  //     if (i == 0) {
  //       continue;
  //     }
  //     if (i == 2 || j != 3) {
  //       // isTerminated[i][j] = true;
  //       // sem_post(&semaphores[i][j]);
  //       // pthread_join(traffic_lights[i][j], NULL);
  //     }      
  //   }
  // }

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

//Todo test main()
// int main(int argc, char const *argv[])
// {
//   int side1 = 3;
//   int direction1 = 2;
//   BlockList test1;
//   sortLockOrder(side1, direction1, &test1);
//   for (int i = 0; i < test1.block_length; i++)
//   {
//     printf("(%d, %d) ", test1.block_side[i], test1.block_direction[i]);
//   }
//   printf("\n");
  
//   free(test1.block_side);
//   free(test1.block_direction);
//   return 0;
// }

// end