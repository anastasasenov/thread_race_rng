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

/* number of threads */
#define TRRND_NUMBER_OF_THREADS 3

/* number of racing steps */
#define TRRND_NUMBER_OF_STEPS TRRND_NUMBER_OF_THREADS

/* RNG control structure */
typedef struct thread_race_rng_t {

    atomic_uint_fast64_t m_uValue[ TRRND_NUMBER_OF_THREADS ];
    atomic_uint_fast64_t m_uPrevValue[ TRRND_NUMBER_OF_THREADS ];
    thrd_t m_tr_threads[ TRRND_NUMBER_OF_THREADS ];
    atomic_bool m_bRunning;
    atomic_uint m_uStep;

} TThreadRaceRNG;

static inline int _get_bit(const uint64_t bits[2], int idx)
{
    return (bits[idx >> 6] >> (idx & 63)) & 1;
}

static inline void _set_bit(uint64_t bits[2], int idx, int value)
{
    if (value) {
        bits[idx >> 6] |= (1ULL << (idx & 63));
    } else {
        bits[idx >> 6] &= ~(1ULL << (idx & 63));
    }
}

static inline uint64_t thread_race_rng_peres_extract(uint64_t uValue1, uint64_t uValue2) 
{
    uint64_t res_lo = 0;
    int res_bit_count = 0;

    uint64_t current[2];
    current[0] = uValue1;
    current[1] = uValue2;

    int current_len = 128;

    while ( (current_len >= 2) && (res_bit_count < 64) ) {

        uint64_t recycled[2] = {0, 0};
        int recycled_count = 0;

        for (int i = 0; i + 1 < current_len; i += 2)
        {
            int x = _get_bit(current, i);
            int y = _get_bit(current, i + 1);

            switch ((x << 1) | y)
            {
                case 0: /* 00 */
                    _set_bit(recycled, recycled_count, 0);
                    recycled_count++;
                    break;

                case 1: /* 01 */
                    if (res_bit_count < 64)
                    {
                        // rest 0
                        res_bit_count++;
                    }
                    break;

                case 2: /* 10 */
                    if (res_bit_count < 64)
                    {
                        res_lo |= (1ULL << res_bit_count);
                        res_bit_count++;
                    }
                    break;

                case 3: /* 11 */
                    _set_bit(recycled, recycled_count, 1);
                    recycled_count++;
                    break;

                default: /* safety */
                    recycled_count++;
                    break;
            }
            
            if (res_bit_count >= 64)
                break;
        }

        current[0] = recycled[0];
        current[1] = recycled[1];
        current_len = recycled_count;
    }

    return res_lo;
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
                uClock |= ( clock() << 32 ); /* winrt portable */
            }

            for (int i = 0; i < TRRND_NUMBER_OF_THREADS; i++) {

                pData->m_uPrevValue[ i ] = pData->m_uValue[ i ];
                pData->m_uValue[ i ] = uClock ^ pData->m_uValue[ i ];
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

    while ( TRRND_NUMBER_OF_STEPS > atomic_load(&(pData->m_uStep)) ) {
        /* avoid entropy Starvation */
        thrd_yield();
    }

    uint64_t uVal = 0;
    uint64_t uPrevVal = 0;
    for (int i = 0; i < TRRND_NUMBER_OF_THREADS; i++) {

        uVal ^= pData->m_uValue[ i ];
        uPrevVal ^= pData->m_uPrevValue[ i ];
    }

    atomic_store( &(pData->m_uStep), 0 );

    return thread_race_rng_peres_extract( uPrevVal, uVal); /* whitening */
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


