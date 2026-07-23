#include "thread_race_rng.h"
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h> 
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#define NUM_OF_T 11
#define NUM_OF_RN ( (uint64_t)1 << 10 )
#define NUM_OF_SAMPLES 100000
#define NUM_BUCKETS 10


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

int test_xi_sq() {

    TThreadRaceRNG rngLoc;

    printf("\n\t2. Xi-sq distribution test ...\n");

    thread_race_rng_init( &rngLoc, time(NULL) ); /* seed */

    double xi_squared = 0.0;
    uint32_t counts[NUM_BUCKETS];
    double expected = (double)NUM_OF_SAMPLES / NUM_BUCKETS; /* expected rates */

    /* init */
    for (int i = 0; i < NUM_BUCKETS; i++) {

        counts[i] = 0;
    }
    
    /* count */
    for (int i = 0; i < NUM_OF_SAMPLES; i++) {

        int number = (thread_race_rng_next( &rngLoc ) % NUM_BUCKETS);
        counts[number] ++;
    }

    // Xi2 = Sum( (O_i - E_i)^2 / E_i )
    for (int i = 0; i < NUM_BUCKETS; i++) {

        double observed = (double)counts[i];
        double diff = observed - expected;
        xi_squared += (diff * diff) / expected;
    }

    thread_race_rng_deinit( &rngLoc );

    const double critical_value = 16.919;
    printf("\t\t\Xi_squared: %f ( critical_value: %f )\n", xi_squared, critical_value);
    assert (xi_squared < critical_value);

    return 0;
}

int test_serial() {
    
    printf("\n\t3. Serial test ( Testing Pairs/Independence ) ...\n");
    
    return 0;
}

int main() {
   
    printf("\n*** thread_race_rng tests ***\n");
    
    thread_race_rng_init( &stRng, 314159 );

    int nRet = test_perf()
        + test_xi_sq()
        + test_serial();
    
    thread_race_rng_deinit( &stRng );

    if ( ! nRet ) printf("\n\tOK\n");
    
    return nRet;
}

