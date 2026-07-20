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
 *      gcc -std=c11 -pthread test.c
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

#define NUMBER_OF_STEPS 7

typedef struct thread_race_rng_t {

    atomic_uint_fast64_t m_uValue[ NUMBER_OF_THREADS ];
    atomic_uint_fast64_t m_uPrevValue[ NUMBER_OF_THREADS ];
    thrd_t m_tr_threads[ NUMBER_OF_THREADS ];
    atomic_bool m_bRunning;
    atomic_uint m_uStep;

} TThreadRaceRNG;

static inline uint64_t thread_race_rng_peres_extract(uint64_t uValue1, uint64_t uValue2) {

    uint64_t outValue1 = 0;
    uint64_t outValue2 = 0;
    int out_bit_count = 0;

    // capacity is 128 bits across two variables
    uint64_t current[2] = { uValue1, uValue2 };
    int current_len = 128;

    // process until we have fewer than 2 bits left
    while (current_len >= 2) {

        uint64_t recycled[2] = {0, 0};
        int recycled_idx = 0;

        // Process the current active bits in pairs
        for (int i = 0; i < current_len; i += 2) {

            // Extract the pair (x, y) from the packed 'current' buffer
            int word_idx_x = i / 64;
            int bit_idx_x  = i % 64;
            int x = (current[word_idx_x] >> bit_idx_x) & 1;

            int word_idx_y = (i + 1) / 64;
            int bit_idx_y  = (i + 1) % 64;
            int y = (current[word_idx_y] >> bit_idx_y) & 1;

            // --- Peres Core Bitwise Logic ---
            // Unbiased bit = x ^ y
            uint64_t u = (uint64_t)(x ^ y);
            // Recycled bit = x
            uint64_t v = (uint64_t)x;

            // Pack the unbiased bit into out_bits
            int out_word = out_bit_count / 64;
            int out_shift = out_bit_count % 64;
            if ( out_bit_count < 64 )
                outValue1 |= (u << out_shift);
            else
                outValue2 |= (u << out_shift);

            out_bit_count++;

            // Pack the recycled bit into the next iteration's buffer
            int rec_word = recycled_idx / 64;
            int rec_shift = recycled_idx % 64;
            recycled[rec_word] |= (v << rec_shift);
            recycled_idx++;
        }

        // The recycled bits become the input for the next loop iteration
        current[0] = recycled[0];
        current[1] = recycled[1];
        current_len = recycled_idx;
    }

    return ( outValue1 ^ outValue2 );
}

static inline uint64_t thread_race_rng_get_mix_time() {

    struct timespec ts;

    timespec_get(&ts, TIME_UTC); // portable C11 function

    return ( ( (uint64_t)ts.tv_sec << 32) ^ (uint64_t)ts.tv_nsec );
}

static inline int thread_race_rng_internal(void * pArg) {

    TThreadRaceRNG * pData;
    pData = ( TThreadRaceRNG * ) pArg;
    
    assert( pData );

    if ( atomic_load(&(pData->m_uStep)) > NUMBER_OF_STEPS ) {

        thrd_yield();
        return 0;
    }

    atomic_fetch_add( &(pData->m_uStep), 1 );
    
    uint64_t uSum;
    
    uSum = thread_race_rng_get_mix_time();
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {

        uSum ^= pData->m_uValue[ i ];
        pData->m_uPrevValue[ i ] = pData->m_uValue[ i ];
        pData->m_uValue[ i ] = uSum ^ pData->m_uValue[ i ];
        pData->m_uValue[ i ] = thread_race_rng_peres_extract(
            pData->m_uPrevValue[ i ], pData->m_uValue[ i ]); /* whitening */
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
    atomic_store( &(pData->m_uStep), 0 );

    // initial
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {

        pData->m_uValue[i] = ( uSeed ^ ( i >> thread_race_rng_get_mix_time() ) );
        pData->m_uPrevValue[i] = ( uSeed ^ thread_race_rng_get_mix_time() );
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

        uRet ^= ( pData->m_uValue[ i ] ^ pData->m_uPrevValue[ i ] );
        pData->m_uPrevValue[ i ] = pData->m_uValue[ i ];
        pData->m_uValue[ i ] = uRet;
    }

    atomic_store( &(pData->m_uStep), 0 );

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
