/* This is SipHash by Jean-Philippe Aumasson and Daniel J. Bernstein.
 * The authors claim it is relatively secure compared to the alternatives
 * and that performance wise it is a suitable hash for languages like Perl.
 * See:
 *
 * https://www.131002.net/siphash/
 *
 * Naming convention:
 *
 * S_perl_hash_siphash_N_M: the N refers to how many rounds are performed per
 * block. The M refers to how many rounds are performed as part of the
 * finalizer. Increased values of either improve security, but decrease
 * performance.
 *
 * _with_state: these functions take a 32 bit state vector prepared by
 * S_perl_siphash_seed_state(). Functions without 'with_state' take a 16
 * byte seed vector and call S_perl_siphash_seed_state() implicitly. If
 * you are hashing many things with the same seed, the _with_state
 * variants are faster.
 *
 * _64: returns a 64 bit hash
 *
 * no-suffix: returns a 32 bit hash.
 *
 * This file defines 9 functions related to implementing 2 variants of
 * the Siphash family of hash functions, Siphash-2-4, and Siphash-1-3.

=for apidoc_section $numeric
=for apidoc   eST|void|S_perl_siphash_seed_state \
                      |const unsigned char * const seed_buf \
                      |unsigned char * state_buf

Takes a 16 byte seed and converts it into a 32 byte state buffer. The
contents of state_buf will be overwritten.

If you need to hash a lot of things, then you can use this to process
the seed once, and then reuse the state over and over.

The siphash functions which take a seed argument will call this function
implicitly every time they are used. Those which take a state argument
require the seed to be converted into a state before they are used.

See the various _with_state siphash functions for a usage example.

=for apidoc   eSTP|U64|S_perl_hash_siphash_1_3_with_state_64\
                      |const unsigned char * const state \
                      |const unsigned char *in|const STRLEN inlen

Implements the variant of Siphash which performs 1 round function
per block, and 3 as part of the finalizer.

Takes a 32 byte 'state' vector prepared by S_perl_siphash_seed_state()
and uses it to hash C<inlen> bytes from the buffer pointed to by C<in>,
returns a 64 bit hash.

The following code should return 0xB70339FD9E758A5C

    U8 state[32];
    char seed[] = "Call me Ishmael.";
    S_perl_siphash_seed_state((const U8*)seed, state);

    char in[] = "It is not down on any map; true places never are.";
    U64 hash = S_perl_hash_siphash_1_3_with_state_64(
                state, (const U8*)in, sizeof(in)-1);

=for apidoc   eSTP|U32|S_perl_hash_siphash_1_3_with_state\
                      |const unsigned char * const state \
                      |const unsigned char *in|const STRLEN inlen

Implements the variant of Siphash which performs 1 round function
per block, and 3 as part of the finalizer.

Takes a 32 byte 'state' vector prepared by S_perl_siphash_seed_state()
and uses it to hash C<inlen> bytes from the buffer pointed to by C<in>,
returns a 32 bit hash.

The following code should return 0x2976B3A1

    U8 state[32];
    char seed[] = "Call me Ishmael.";
    S_perl_siphash_seed_state((const U8*)seed, state);

    char in[] = "It is not down on any map; true places never are.";
    U32 hash = S_perl_hash_siphash_1_3_with_state(
                state, (const U8*)in, sizeof(in)-1);

=for apidoc   eSTP|U64|S_perl_hash_siphash_1_3_64\
                      |const unsigned char * const seed \
                      |const unsigned char *in|const STRLEN inlen

Implements the variant of Siphash which performs 1 round function
per block, and 3 as part of the finalizer.

Takes a 16 byte C<seed> vector, and uses it to hash C<inlen> bytes
from the buffer pointed to by C<in>, returns a 64 bit hash.

The following code should return 0xB70339FD9E758A5C

    char seed[] = "Call me Ishmael.";
    char in[] = "It is not down on any map; true places never are.";
    U64 hash = S_perl_hash_siphash_1_3_64(
                (const U8*)seed, (const U8*)in, sizeof(in)-1);

=for apidoc   eSTP|U64|S_perl_hash_siphash_1_3\
                      |const unsigned char * const seed \
                      |const unsigned char *in|const STRLEN inlen

Implements the variant of Siphash which performs 1 round function
per block, and 3 as part of the finalizer.

Takes a 16 byte C<seed> vector, and uses it to hash C<inlen> bytes
from the buffer pointed to by C<in>, returns a 32 bit hash.

The following code should return 0x2976B3A1

    char seed[] = "Call me Ishmael.";
    char in[] = "It is not down on any map; true places never are.";
    U32 hash = S_perl_hash_siphash_1_3(
                (const U8*)seed, (const U8*)in, sizeof(in)-1);

=for apidoc   eSTP|U64|S_perl_hash_siphash_2_4_with_state_64\
                      |const unsigned char * const state \
                      |const unsigned char *in|const STRLEN inlen

Implements the variant of Siphash which performs 2 round functions
per block, and 4 as part of the finalizer.

Takes a 32 byte 'state' vector prepared by S_perl_siphash_seed_state()
and uses it to hash C<inlen> bytes from the buffer pointed to by C<in>,
returns a 64 bit hash.

The following code should return 0x1E84CF1D7AA516B7

    U8 state[32];
    char seed[] = "Call me Ishmael.";
    S_perl_siphash_seed_state((const U8*)seed, state);

    char in[] = "It is not down on any map; true places never are.";
    U64 hash = S_perl_hash_siphash_2_4_with_state_64(
                state, (const U8*)in, sizeof(in)-1);

=for apidoc   eSTP|U32|S_perl_hash_siphash_2_4_with_state\
                      |const unsigned char * const state \
                      |const unsigned char *in|const STRLEN inlen

Implements the variant of Siphash which performs 2 round function
per block, and 4 as part of the finalizer.

Takes a 32 byte 'state' vector prepared by S_perl_siphash_seed_state()
and uses it to hash C<inlen> bytes from the buffer pointed to by C<in>,
returns a 32 bit hash.

The following code should return 0x6421D9AA

    U8 state[32];
    char seed[] = "Call me Ishmael.";
    S_perl_siphash_seed_state((const U8*)seed, state);

    char in[] = "It is not down on any map; true places never are.";
    U32 hash = S_perl_hash_siphash_2_4_with_state(
                state, (const U8*)in, sizeof(in)-1);

=for apidoc   eSTP|U64|S_perl_hash_siphash_2_4_64\
                      |const unsigned char * const seed \
                      |const unsigned char *in|const STRLEN inlen

Implements the variant of Siphash which performs 2 round functions
per block, and 4 as part of the finalizer.

Takes a 16 byte C<seed> vector, and uses it to hash C<inlen> bytes
from the buffer pointed to by C<in>, returns a 64 bit hash.

The following code should return 0x1E84CF1D7AA516B7

    char seed[] = "Call me Ishmael.";
    char in[] = "It is not down on any map; true places never are.";
    U64 hash = S_perl_hash_siphash_2_4_64(
                (const U8*)seed, (const U8*)in, sizeof(in)-1);

=for apidoc   eSTP|U32|S_perl_hash_siphash_2_4\
                      |const unsigned char * const seed \
                      |const unsigned char *in|const STRLEN inlen

Implements the variant of Siphash which performs 2 round functions
per block, and 4 as part of the finalizer.

Takes a 16 byte C<seed> vector, and uses it to hash C<inlen> bytes
from the buffer pointed to by C<in>, returns a 32 bit hash.

The following code should return 0x6421D9AA

    char seed[] = "Call me Ishmael.";
    char in[] = "It is not down on any map; true places never are.";
    U32 hash = S_perl_hash_siphash_2_4(
                (const U8*)seed, (const U8*)in, sizeof(in)-1);

=cut
*/

#ifdef CAN64BITHASH

#define SIPROUND            \
  STMT_START {              \
    v0 += v1; v1=ROTL64(v1,13); v1 ^= v0; v0=ROTL64(v0,32); \
    v2 += v3; v3=ROTL64(v3,16); v3 ^= v2;     \
    v0 += v3; v3=ROTL64(v3,21); v3 ^= v0;     \
    v2 += v1; v1=ROTL64(v1,17); v1 ^= v2; v2=ROTL64(v2,32); \
  } STMT_END

#define SIPHASH_SEED_STATE(key,v0,v1,v2,v3) \
do {                                    \
    v0 = v2 = U8TO64_LE(key + 0);       \
    v1 = v3 = U8TO64_LE(key + 8);       \
  /* "somepseudorandomlygeneratedbytes" */  \
    v0 ^= UINT64_C(0x736f6d6570736575);  \
    v1 ^= UINT64_C(0x646f72616e646f6d);      \
    v2 ^= UINT64_C(0x6c7967656e657261);      \
    v3 ^= UINT64_C(0x7465646279746573);      \
} while (0)

PERL_STATIC_INLINE
void S_perl_siphash_seed_state(const unsigned char * const seed_buf, unsigned char * state_buf) {
    U64 *v= (U64*) state_buf;
    SIPHASH_SEED_STATE(seed_buf, v[0],v[1],v[2],v[3]);
}

#define PERL_SIPHASH_FNC(FNC,SIP_ROUNDS,SIP_FINAL_ROUNDS) \
PERL_STATIC_INLINE U64 \
FNC ## _with_state_64 \
  (const unsigned char * const state, const unsigned char *in, const STRLEN inlen) \
{                                   \
  const int left = inlen & 7;       \
  const U8 *end = in + inlen - left;\
                                    \
  U64 b = ( ( U64 )(inlen) ) << 56; \
  U64 m;                            \
  U64 v0 = U8TO64_LE(state);        \
  U64 v1 = U8TO64_LE(state+8);      \
  U64 v2 = U8TO64_LE(state+16);     \
  U64 v3 = U8TO64_LE(state+24);     \
                                    \
  for ( ; in != end; in += 8 )      \
  {                                 \
    m = U8TO64_LE( in );            \
    v3 ^= m;                        \
                                    \
    SIP_ROUNDS;                     \
                                    \
    v0 ^= m;                        \
  }                                 \
                                    \
  switch( left )                    \
  {                                 \
  case 7: b |= ( ( U64 )in[ 6] )  << 48; /*FALLTHROUGH*/    \
  case 6: b |= ( ( U64 )in[ 5] )  << 40; /*FALLTHROUGH*/    \
  case 5: b |= ( ( U64 )in[ 4] )  << 32; /*FALLTHROUGH*/    \
  case 4: b |= ( ( U64 )in[ 3] )  << 24; /*FALLTHROUGH*/    \
  case 3: b |= ( ( U64 )in[ 2] )  << 16; /*FALLTHROUGH*/    \
  case 2: b |= ( ( U64 )in[ 1] )  <<  8; /*FALLTHROUGH*/    \
  case 1: b |= ( ( U64 )in[ 0] ); break;    \
  case 0: break;            \
  }                         \
                            \
  v3 ^= b;                  \
                            \
  SIP_ROUNDS;               \
                            \
  v0 ^= b;                  \
                            \
  v2 ^= 0xff;               \
                            \
  SIP_FINAL_ROUNDS          \
                            \
  b = v0 ^ v1 ^ v2  ^ v3;   \
  return b;                 \
}                           \
                            \
PERL_STATIC_INLINE U32      \
FNC ## _with_state          \
  (const unsigned char * const state, const unsigned char *in, const STRLEN inlen) \
{                           \
    union {                 \
        U64 h64;            \
        U32 h32[2];         \
    } h;                    \
    h.h64= FNC ## _with_state_64(state,in,inlen); \
    return h.h32[0] ^ h.h32[1]; \
}                           \
                            \
                            \
PERL_STATIC_INLINE U32      \
FNC (const unsigned char * const seed, const unsigned char *in, const STRLEN inlen) \
{                           \
    U64 state[4];           \
    SIPHASH_SEED_STATE(seed,state[0],state[1],state[2],state[3]);   \
    return FNC ## _with_state((U8*)state,in,inlen);                 \
}                           \
                            \
PERL_STATIC_INLINE U64      \
FNC ## _64 (const unsigned char * const seed, const unsigned char *in, const STRLEN inlen) \
{                           \
    U64 state[4];           \
    SIPHASH_SEED_STATE(seed,state[0],state[1],state[2],state[3]);   \
    return FNC ## _with_state_64((U8*)state,in,inlen);              \
}

PERL_SIPHASH_FNC(
    S_perl_hash_siphash_1_3
    ,SIPROUND;
    ,SIPROUND;SIPROUND;SIPROUND;
)

PERL_SIPHASH_FNC(
    S_perl_hash_siphash_2_4
    ,SIPROUND;SIPROUND;
    ,SIPROUND;SIPROUND;SIPROUND;SIPROUND;
)

#endif /* defined(CAN64BITHASH) */
