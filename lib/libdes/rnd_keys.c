/*	$OpenBSD: rnd_keys.c,v 1.2 2000/03/02 00:29:48 todd Exp $	*/

/* Copyright (C) 1993 Eric Young - see COPYING for more details */
#include "des_locl.h"
#include <sys/time.h>

#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/*
 * Create a sequence of random 64 bit blocks.
 * The sequence is indexed with a long long and 
 * based on an initial des key used as a seed.
 */
static des_key_schedule sequence_seed;
static u_int32_t sequence_index[2];

/*
 * In case the generator does not get inited use this for backup.
 */
static int initialized;
static des_cblock default_seed = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
static void
do_initialize()
{
  des_set_odd_parity(&default_seed);
  des_set_random_generator_seed(&default_seed);
}

#define zero_long_long(ll) do { ll[0] = ll[1] = 0; } while (0)

#define incr_long_long(ll) do { if (++ll[0] == 0) ++ll[1]; } while (0)

#define des_set_sequence_number(ll) \
do { \
       memcpy((char *)sequence_index, (ll), sizeof(sequence_index)); \
     } while (0)

#define des_generate_random_block(ret) \
do { \
       des_ecb_encrypt((des_cblock *) sequence_index, (ret), sequence_seed, DES_ENCRYPT); \
	 incr_long_long(sequence_index); \
     } while (0)

void
des_set_random_generator_seed(des_cblock *seed)
{
  des_key_sched(seed, sequence_seed);
  zero_long_long(sequence_index);
  initialized = 1;
}

/*
 * Generate a sequence of random des keys
 * using the random block sequence, fixup
 * parity and skip weak keys.
 */
int
des_new_random_key(des_cblock *key)
{
  if (!initialized)
    do_initialize();

 try_again:
  des_generate_random_block(key);
  /* random key must have odd parity and not be weak */
  des_set_odd_parity(key);
  if (des_is_weak_key(key))
    goto try_again;
  return(0);
}

/*
 * des_init_random_number_generator:
 *
 *    This routine takes a secret key possibly shared by a number
 * of servers and uses it to generate a random number stream that is
 * not shared by any of the other servers.  It does this by using the current
 * process id, host id, and the current time to the nearest second.  The
 * resulting stream seed is not useful information for cracking the secret
 * key.   Moreover, this routine keeps no copy of the secret key.
 *
 */
void 
des_init_random_number_generator(des_cblock *seed)
{
  struct timeval now;
  static long uniq[2];
  des_cblock new_key;
  long gethostid(void);

  gettimeofday(&now, (struct timezone *)0);
  if (!uniq[0])
    {
      struct hostent *hent;
      char hostname[100];
      gethostname(hostname, sizeof(hostname));
      hent = gethostbyname(hostname);
      if (hent != NULL)
	bcopy(hent->h_addr_list[0], &uniq[0], sizeof(uniq[0]));
      else
	uniq[0] = gethostid();
#ifdef MSDOS
      uniq[1] = 1;
#else
      uniq[1] = getpid();
#endif
    }

  /* Pick a unique random key from the shared sequence. */
  des_set_random_generator_seed(seed);
  des_set_sequence_number((unsigned char *)uniq);
  des_new_random_key(&new_key);

  /* Select a new nonshared sequence, */
  des_set_random_generator_seed(&new_key);

  /* and use the current time to pick a key for the new sequence. */
  des_set_sequence_number((unsigned char *)&now);
  des_new_random_key(&new_key);
  des_set_random_generator_seed(&new_key);
}
