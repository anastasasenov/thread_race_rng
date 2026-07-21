# Thread race RNG

This project presents an experimental, software-based True Random Number Generator (TRNG). By exploiting CPU execution non-determinism and thread race conditions, the system generates high-speed random numbers without relying on complex mathematical functions.

## API

```
/** Init RNG */
void thread_race_rng_init(TThreadRaceRNG * pData, uint64_t uSeed)

/** Deinit RNG */
void thread_race_rng_deinit(TThreadRaceRNG * pData)

/** Get random value */
uint64_t thread_race_rng_next(TThreadRaceRNG * pData)

```

## Architecture

Multiple threads modify shared data + steady clock -> Whitening ( Peres extractor ) -> Random numbers

## An example

```
#include "thread_race_rng.h"

TThreadRaceRNG stRng;
thread_race_rng_init( &stRng, 314159 );
int random_value = thread_race_rng_next( &stRng );
thread_race_rng_deinit( &stRng );
```

## Notes

Since this project is experimental, we can explore several approaches.

* try BRAKE3 whitening
* use more/less threads
* add Runtime Continuous Test
* use TLS ( thread local storage )
* add Fail-Safe design implementation
* avoid Cache Line Bouncing

## License

This project is open-source and available under the MIT License.
