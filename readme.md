# Thread race RNG

An experimental random number generator (RNG) based on thread race conditions.

## API

```
/** Init RNG */
void thread_race_rng_init(TThreadRaceRNG * pData, uint64_t uSeed)

/** Deinit RNG */
void thread_race_rng_deinit(TThreadRaceRNG * pData)

/** Get random value */
uint64_t thread_race_rng_next(TThreadRaceRNG * pData)

```

## An example

```
#include "thread_race_rng.h"

TThreadRaceRNG stRng;
thread_race_rng_init( &stRng, 314159 );
int random_value = thread_race_rng_next( &stRng );
thread_race_rng_deinit( &stRng );
```

## License

This project is open-source and available under the MIT License.
