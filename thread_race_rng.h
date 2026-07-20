/**
 * An experimental random number generator (RNG)
 * based on thread race conditions
 * 
 * Usage:
 * 
 *      TThreadRaceRNG stRng;
 *      thread_race_rng_init( &stRng, 314159 );
 *      int random_value = thread_race_rng_next( &stRng );
 *      thread_race_rng_deinit( &stRng );
 * 
 * Compule with C11 support:
 * 
 *  gcc -std=c11 test.c
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <threads.h> // C11 threads
#include <time.h>
#include <assert.h>

// number of thread
#define NUMBER_OF_THREADS 3

typedef struct thread_race_rng_t {

    atomic_uint_fast64_t m_uValue[ NUMBER_OF_THREADS ];
    thrd_t m_tr_threads[ NUMBER_OF_THREADS ];
    atomic_bool m_bRunning;

} TThreadRaceRNG;

static uint64_t thread_race_rng_get_mix_time() {

    struct timespec ts;

    timespec_get(&ts, TIME_UTC); // portable C11 function

    return ( ( (uint64_t)ts.tv_sec << 32) ^ (uint64_t)ts.tv_nsec );
}

static int thread_race_rng_internal(void * pArg) {

    TThreadRaceRNG * pData;
    pData = ( TThreadRaceRNG * ) pArg;
    
    assert( pData );
    
    uint64_t uSum;
    
    uSum = thread_race_rng_get_mix_time();
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {

        uSum ^= pData->m_uValue[ i ];
        pData->m_uValue[ i ] = uSum ^ pData->m_uValue[ i ];
    }  
    
    return 0;
}

// Public API

/**
 * Init generator and launch ISO C11 threads
 */
static inline void thread_race_rng_init(TThreadRaceRNG * pData, uint64_t uSeed) {

    
    assert( pData );
    
    atomic_store( &(pData->m_bRunning), false );

    // initial
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {

        pData->m_uValue[i] = ( uSeed ^ ( i >> thread_race_rng_get_mix_time() ) );
    }

    // launch threads
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {

        pData->m_tr_threads[i] = 0;
        thrd_create( &(pData->m_tr_threads[i]), thread_race_rng_internal, pData);
        assert( pData->m_tr_threads[i] );
    }
    
    atomic_store( &(pData->m_bRunning), true );
}

/**
 * Get random number. Lock-free.
 */
static inline uint64_t thread_race_rng_next(TThreadRaceRNG * pData) {

    assert( pData );
    assert( pData->m_bRunning );

    uint64_t uRet;

    uRet = 0;
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {

        uRet ^= pData->m_uValue[ i ];
        pData->m_uValue[ i ] ^= ( uRet << i );
    }

    return uRet;
}

/**
 * Stop threads - thrd_join.
 */
static inline void thread_race_rng_deinit(TThreadRaceRNG * pData) {

    assert( pData );

    if ( ! atomic_load(&(pData->m_bRunning)) ) {
        return;
    }

    atomic_store( &(pData->m_bRunning), false);

    // waiting for join
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {

        thrd_join( pData->m_tr_threads[i], NULL );
    }
}
