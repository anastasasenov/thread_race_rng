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
#define TRRND_NUMBER_OF_THREADS 3

#define TRRND_NUMBER_OF_STEPS TRRND_NUMBER_OF_THREADS

typedef struct thread_race_rng_t {

    atomic_uint_fast64_t m_uValue[ TRRND_NUMBER_OF_THREADS ];
    atomic_uint_fast64_t m_uPrevValue[ TRRND_NUMBER_OF_THREADS ];
    thrd_t m_tr_threads[ TRRND_NUMBER_OF_THREADS ];
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

static inline int thread_race_rng_internal(void * pArg) {

    TThreadRaceRNG * pData;
    pData = ( TThreadRaceRNG * ) pArg;
    
    assert( pData );

    while ( atomic_load(&(pData->m_bRunning)) ) {

        if ( atomic_load(&(pData->m_uStep)) > TRRND_NUMBER_OF_STEPS ) {

            thrd_yield();

        } else {

            /* steady timer */
            uint64_t uClock = clock();
            if ( sizeof(clock_t) < sizeof(uint64_t) ) {
                uClock += ( clock() << sizeof(clock_t) ); /* winrt portable */
            }

            for (int i = 0; i < TRRND_NUMBER_OF_THREADS; i++) {

                pData->m_uPrevValue[ i ] = pData->m_uValue[ i ];
                pData->m_uValue[ i ] = uClock ^ pData->m_uValue[ i ];
                pData->m_uValue[ i ] = thread_race_rng_peres_extract(
                    pData->m_uPrevValue[ i ], pData->m_uValue[ i ]); /* whitening */
            }
            
            atomic_fetch_add( &(pData->m_uStep), 1 );
        }
    }
    
    return 0;
}

/** Public API */

/**
 * Init generator and launch ISO C11 threads
 */
static inline void thread_race_rng_init(TThreadRaceRNG * pData, uint64_t uSeed) {

    assert( pData );
    
    /* initial */
    atomic_store( &(pData->m_bRunning), true );
    atomic_store( &(pData->m_uStep), 0 );
    for (int i = 0; i < TRRND_NUMBER_OF_THREADS; i++) {

        pData->m_uValue[i] = uSeed;
        pData->m_uPrevValue[i] = 0;
    }

    /* launch threads */
    for (int i = 0; i < TRRND_NUMBER_OF_THREADS; i++) {

        pData->m_tr_threads[i] = 0;
        thrd_create( &(pData->m_tr_threads[i]), thread_race_rng_internal, pData);
        assert( pData->m_tr_threads[i] );
    }
}

/**
 * Get random number. Lock-free.
 */
static inline uint64_t thread_race_rng_next(TThreadRaceRNG * pData) {

    assert( pData );
    assert( pData->m_bRunning );

    uint64_t uRet = 0;

    while ( TRRND_NUMBER_OF_STEPS > atomic_load(&(pData->m_uStep)) ) {
        /* avoid entropy Starvation */
        thrd_yield();
    }

    for (int i = 0; i < TRRND_NUMBER_OF_THREADS; i++) {

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

    if ( atomic_load(&(pData->m_bRunning)) ) {
        
        atomic_store( &(pData->m_bRunning), false);

        /* waiting for join */
        for (int i = 0; i < TRRND_NUMBER_OF_THREADS; i++) {

            thrd_join( pData->m_tr_threads[i], NULL );
        }
    }
}

