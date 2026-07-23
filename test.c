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
#define NUM_SER_BUCKETS 4
#define NUM_BYTES NUM_OF_SAMPLES
#define TOTAL_BITS NUM_OF_SAMPLES

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

    // less than 3 seconds
    assert ( (tsEnd.tv_sec - tsStart.tv_sec) < 3 );
    
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
    printf("\t\t Xi_squared: %f ( less than critical_value: %f )\n", xi_squared, critical_value);
    assert (xi_squared < critical_value);

    return 0;
}

int test_serial() {
    
    TThreadRaceRNG rngLoc;
    int pair_counts[NUM_SER_BUCKETS][NUM_SER_BUCKETS]; // 2D array
    
    printf("\n\t3. Serial test ( Testing Pairs/Independence ) ...\n");
    
    thread_race_rng_init( &rngLoc, time(NULL) ); /* seed */

    /* init */
    for (int i = 0; i < NUM_SER_BUCKETS; i ++) {
        for (int j = 0; j < NUM_SER_BUCKETS; j ++) {
            pair_counts[i][j] = 0;
        }
    }

    int current = (thread_race_rng_next( &rngLoc ) % NUM_SER_BUCKETS);
    for (int i = 0; i < NUM_OF_SAMPLES; i++) {
        int next = (thread_race_rng_next( &rngLoc ) % NUM_SER_BUCKETS);
        pair_counts[current][next] ++;
        current = next;
    }
    
    double expected = (double)NUM_OF_SAMPLES / (NUM_SER_BUCKETS * NUM_SER_BUCKETS);
    double xi_squared = 0.0;
    
    for (int i = 0; i < NUM_SER_BUCKETS; i ++) {
        for (int j = 0; j < NUM_SER_BUCKETS; j ++) {
            double observed = pair_counts[i][j];
            double diff = observed - expected;
            xi_squared += (diff * diff) / expected;
        }
    }

    thread_race_rng_deinit( &rngLoc );

    const double critical_value = 25.0;
    printf("\t\t Xi_squared: %f ( less than critical_value: %f )\n", xi_squared, critical_value);
    assert (xi_squared < critical_value);

    return 0;
}

uint8_t test_monobit_byte_rng(TThreadRaceRNG * p) {

    return (uint8_t)(thread_race_rng_next(p) % 256);
}

int test_monobit() {
    
    TThreadRaceRNG rngLoc;

    printf("\n\t4. Monobit Test (Frequency Test at Bit Level) ...\n");

    thread_race_rng_init( &rngLoc, time(NULL) ); /* seed */

    int Sn = 0; /* sum: +1 for bit '1', -1 for bit '0' */
    
    for (int i = 0; i < NUM_BYTES; i++) {

        uint8_t byte = test_monobit_byte_rng( &rngLoc );
        for (int b = 0; b < 8; b++) {
            if ((byte >> b) & 1) {
                Sn += 1;
            } else {
                Sn -= 1;
            }
        }
    }

    int total_bits = NUM_BYTES * 8;
    double s_obs = abs(Sn) / sqrt(total_bits);
    double p_value = erfc(s_obs / sqrt(2.0));
    
    thread_race_rng_deinit( &rngLoc );

    const double critical_value = 0.1;
    printf("\t\t p_value: %f ( less than critical_value: %f )\n", p_value, critical_value);
    assert (p_value < critical_value);

    return 0;
}

bool test_monobit_bit_rng(TThreadRaceRNG * p) {

    return (1 == (thread_race_rng_next(p) % 2));
}

int test_oscillation() {

    TThreadRaceRNG rngLoc;
    bool bits[TOTAL_BITS];
    double ones_count = 0;

    printf("\n\t5. Testing Oscillation ...\n");

    thread_race_rng_init( &rngLoc, time(NULL) );

   for (int i = 0; i < TOTAL_BITS; i++) {
        bits[i] = test_monobit_bit_rng( &rngLoc );
        if (bits[i]) ones_count++;
    }
    
    thread_race_rng_deinit( &rngLoc );

    double pi = ones_count / TOTAL_BITS;
    
    assert( fabs(pi - 0.5) < (2.0 / sqrt(TOTAL_BITS)));
    
    int runs = 1; 
    for (int i = 0; i < TOTAL_BITS - 1; i++) {
        if (bits[i] != bits[i+1]) {
            runs++;
        }
    }
  
    double num = fabs((double)runs - (2.0 * TOTAL_BITS * pi * (1.0 - pi)));
    double den = 2.0 * sqrt(2.0 * TOTAL_BITS) * pi * (1.0 - pi);
    double p_value = erfc(num / den);

    const double critical_value = 0.01;
    printf("\t\t p_value: %f ( more than critical_value: %f )\n", p_value, critical_value);
    assert (p_value >= critical_value);

    return 0;
}

int main() {
   
    printf("\n*** thread_race_rng tests ***\n");
    
    thread_race_rng_init( &stRng, 314159 );

    int nRet = test_perf()
        + test_xi_sq()
        + test_serial()
        + test_monobit()
        + test_oscillation();
    
    thread_race_rng_deinit( &stRng );

    if ( ! nRet ) printf("\n\tOK\n");
    
    return nRet;
}
