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
    if (idx < 0 || idx >= 128) return 0;

    return (int)((bits[idx >> 6] >> (idx & 63)) & 1ULL);
}

static inline void _set_bit(uint64_t bits[2], int idx, int value)
{
    if (idx < 0 || idx >= 128) return;
    
    uint64_t mask = 1ULL << (idx & 63);

    if (value)
        bits[idx >> 6] |= mask;
    else
        bits[idx >> 6] &= ~mask;
}

static void _peres_extract_recursive(
    const uint64_t input[2],
    int input_len,
    uint64_t *result,
    int *result_bits)
{
    if (input_len < 2 || *result_bits >= 64)
        return;

    uint64_t u_stream[2] = { 0, 0 };
    uint64_t v_stream[2] = { 0, 0 };
    int u_len = 0;
    int v_len = 0;

    for (int i = 0; i + 1 < input_len; i += 2)
    {
        if (*result_bits >= 64)
            break;

        int x = _get_bit(input, i);
        int y = _get_bit(input, i + 1);

        if (x != y)
        {
            if (x == 1) {
                *result |= (1ULL << *result_bits);
            }
            (*result_bits)++;

            _set_bit(u_stream, u_len++, 1);
        }
        else
        {
            _set_bit(u_stream, u_len++, 0);

            _set_bit(v_stream, v_len++, x);
        }
    }

    _peres_extract_recursive(u_stream, u_len, result, result_bits);

    _peres_extract_recursive(v_stream, v_len, result, result_bits);
}

static inline uint64_t _thread_race_rng_peres_extract(
    uint64_t value1,
    uint64_t value2)
{
    uint64_t result = 0;
    int result_bits = 0;
    uint64_t input[2] = { value1, value2 };

    _peres_extract_recursive(input, 128, &result, &result_bits);

    return result;
}

static inline int _thread_race_rng_internal(void * pArg) {

    TThreadRaceRNG * pData;
    pData = ( TThreadRaceRNG * ) pArg;
    
    assert( pData );

    while ( atomic_load(&(pData->m_bRunning)) ) {

        if ( atomic_load(&(pData->m_uStep)) > TRRND_NUMBER_OF_STEPS ) {

            thrd_yield();

        } else {

            struct timespec ts;
            timespec_get(&ts, TIME_UTC);

            uint64_t uClock = clock(); /* steady timer */
            uClock = uClock << 32 | ts.tv_nsec;
            for (int i = 0; i < TRRND_NUMBER_OF_THREADS; i++) {

                thrd_yield();
                pData->m_uPrevValue[ i ] = pData->m_uValue[ i ];
                thrd_yield();
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
        thrd_create( &(pData->m_tr_threads[i]), _thread_race_rng_internal, pData);
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

    return _thread_race_rng_peres_extract( uPrevVal, uVal); /* whitening */
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

