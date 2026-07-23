#include "thread_race_rng.h"
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h> 
#include <assert.h>

#define NUM_OF_T 11
#define NUM_OF_RN ( (uint64_t)1 << 10 )

TThreadRaceRNG stRng;
uint64_t random_value = 0;

int th_fn_test_perf(void* p) {

    for (uint64_t i = 0; i < NUM_OF_RN; i ++ ) {
        random_value += thread_race_rng_next( &stRng );
    }
        
    return ( p ? 1 : 0 );
}

int test_perf() {

    struct timespec tsStart;
    struct timespec tsEnd;

    printf("\n\t1. Performance test ...\n");

    timespec_get(&tsStart, TIME_UTC);
    
    thrd_t thread_id[NUM_OF_T];

    for (int t = 0; t < NUM_OF_T; t ++) {
        
        thrd_create(&(thread_id[t]), th_fn_test_perf, NULL);
    }

    uint64_t i = 0;
    for (i = 0; i < NUM_OF_RN; i ++ ) {
        random_value += thread_race_rng_next( &stRng );
    }

    for (int t = 0; t < NUM_OF_T; t ++) {
        
        thrd_join(thread_id[t], NULL);
    }
   
    int nProduced = i * (NUM_OF_T + 1);
    printf("\t\tproduced: %d randoms ( avg: %u )\n",
        nProduced, (unsigned int)(random_value / nProduced) );
    
    timespec_get(&tsEnd, TIME_UTC);

    // less than 2 seconds
    assert ( (tsEnd.tv_sec - tsStart.tv_sec) < 2 );
    
    return 0;
}

int test_normal_distribution() {
    
    printf("\n\t2. Normal distribution test ...\n");

    return 0;
}

int main() {
   
    printf("\n*** thread_race_rng tests ***\n");
    
    thread_race_rng_init( &stRng, 314159 );

    int nRet = test_perf()
        + test_normal_distribution();
    
    thread_race_rng_deinit( &stRng );

    if ( ! nRet ) printf("\n\tOK\n");
    
    return nRet;
}
