/*

random.h

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Sat Mar  4 14:49:05 1995 ylo

Cryptographically strong random number generator.

*/

/* RCSID("$Id: randoms.h,v 1.1 1999/09/26 20:53:37 deraadt Exp $"); */

#ifndef RANDOM_H
#define RANDOM_H

#include "ssh_md5.h"

#define RANDOM_STATE_BITS	8192
#define RANDOM_STATE_BYTES	(RANDOM_STATE_BITS / 8)

/* Structure for the random state. */
typedef struct
{
  unsigned char state[RANDOM_STATE_BYTES];/* Pool of random data. */
  unsigned char stir_key[64];		/* Extra data for next stirring. */
  unsigned int next_available_byte;	/* Index of next available byte. */
  unsigned int add_position;		/* Index to add noise. */
} RandomState;

/* Initializes the random number generator, loads any random information
   from the given file, and acquires as much environmental noise as it
   can to initialize the random number generator.  More noise can be
   acquired later by calling random_add_noise + random_stir, or by
   calling random_get_environmental_noise again later when the environmental
   situation has changed. */
void random_initialize(RandomState *state, const char *filename);

/* Acquires as much environmental noise as it can.  This is probably quite
   sufficient on a unix machine, but might be grossly inadequate on a
   single-user PC or a Macintosh.  This call random_stir automatically. 
   This call may take many seconds to complete on a busy system. */
void random_acquire_environmental_noise(RandomState *state);

/* Acquires easily available noise from the environment. */
void random_acquire_light_environmental_noise(RandomState *state);

/* Executes the given command, and processes its output as noise.
   random_stir should be called after this. */
void random_get_noise_from_command(RandomState *state, const char *cmd);

/* Adds the contents of the buffer as noise.  random_stir should be called
   after this. */
void random_add_noise(RandomState *state, const void *buf, unsigned int bytes);

/* Stirs the random pool to consume any newly acquired noise or to get more
   random numbers.  This should be called after adding noise to properly
   mix the noise into the random pool. */
void random_stir(RandomState *state);

/* Returns a random byte.  Stirs the random pool if necessary.  Acquires
   new environmental noise approximately every five minutes. */
unsigned int random_get_byte(RandomState *state);

/* Saves some random bits in the file so that it can be used as a source
   of randomness for later runs. */
void random_save(RandomState *state, const char *filename);

/* Zeroes and frees any data structures associated with the random number
   generator. */
void random_clear(RandomState *state);

#endif /* RANDOM_H */
