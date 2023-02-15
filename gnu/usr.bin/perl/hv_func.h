/* hash a key
 *--------------------------------------------------------------------------------------
 * The "hash seed" feature was added in Perl 5.8.1 to perturb the results
 * to avoid "algorithmic complexity attacks".
 *
 * If USE_HASH_SEED is defined, hash randomisation is done by default
 * (see also perl.c:perl_parse() and S_init_tls_and_interp() and util.c:get_hash_seed())
 */
#ifndef PERL_SEEN_HV_FUNC_H_ /* compile once */
#define PERL_SEEN_HV_FUNC_H_
#include "hv_macro.h"

#if !( 0 \
        || defined(PERL_HASH_FUNC_SIPHASH) \
        || defined(PERL_HASH_FUNC_SIPHASH13) \
        || defined(PERL_HASH_FUNC_ZAPHOD32) \
    )
#   ifdef CAN64BITHASH
#       define PERL_HASH_FUNC_SIPHASH13
#   else
#       define PERL_HASH_FUNC_ZAPHOD32
#   endif
#endif

#ifndef PERL_HASH_USE_SBOX32_ALSO
#define PERL_HASH_USE_SBOX32_ALSO 1
#endif

#ifndef SBOX32_MAX_LEN
#define SBOX32_MAX_LEN 24
#endif

/* this must be after the SBOX32_MAX_LEN define */
#include "sbox32_hash.h"

#if defined(PERL_HASH_FUNC_SIPHASH)
# define __PERL_HASH_FUNC "SIPHASH_2_4"
# define __PERL_HASH_WORD_TYPE U64
# define __PERL_HASH_WORD_SIZE sizeof(__PERL_HASH_WORD_TYPE)
# define __PERL_HASH_SEED_BYTES (__PERL_HASH_WORD_SIZE * 2)
# define __PERL_HASH_STATE_BYTES (__PERL_HASH_WORD_SIZE * 4)
# define __PERL_HASH_SEED_STATE(seed,state) S_perl_siphash_seed_state(seed,state)
# define __PERL_HASH_WITH_STATE(state,str,len) S_perl_hash_siphash_2_4_with_state((state),(U8*)(str),(len))
#elif defined(PERL_HASH_FUNC_SIPHASH13)
# define __PERL_HASH_FUNC "SIPHASH_1_3"
# define __PERL_HASH_WORD_TYPE U64
# define __PERL_HASH_WORD_SIZE sizeof(__PERL_HASH_WORD_TYPE)
# define __PERL_HASH_SEED_BYTES (__PERL_HASH_WORD_SIZE * 2)
# define __PERL_HASH_STATE_BYTES (__PERL_HASH_WORD_SIZE * 4)
# define __PERL_HASH_SEED_STATE(seed,state) S_perl_siphash_seed_state(seed,state)
# define __PERL_HASH_WITH_STATE(state,str,len) S_perl_hash_siphash_1_3_with_state((state),(U8*)(str),(len))
#elif defined(PERL_HASH_FUNC_ZAPHOD32)
# define __PERL_HASH_FUNC "ZAPHOD32"
# define __PERL_HASH_WORD_TYPE U32
# define __PERL_HASH_WORD_SIZE sizeof(__PERL_HASH_WORD_TYPE)
# define __PERL_HASH_SEED_BYTES (__PERL_HASH_WORD_SIZE * 3)
# define __PERL_HASH_STATE_BYTES (__PERL_HASH_WORD_SIZE * 3)
# define __PERL_HASH_SEED_STATE(seed,state) zaphod32_seed_state(seed,state)
# define __PERL_HASH_WITH_STATE(state,str,len) (U32)zaphod32_hash_with_state((state),(U8*)(str),(len))
# include "zaphod32_hash.h"
#endif

#ifndef __PERL_HASH_WITH_STATE
#error "No hash function defined!"
#endif
#ifndef __PERL_HASH_SEED_BYTES
#error "__PERL_HASH_SEED_BYTES not defined"
#endif
#ifndef __PERL_HASH_FUNC
#error "__PERL_HASH_FUNC not defined"
#endif

/* Some siphash static functions are needed by XS::APItest even when
   siphash isn't the current hash.  For SipHash builds this needs to
   be before the S_perl_hash_with_seed() definition.
*/
#include "perl_siphash.h"

#define __PERL_HASH_SEED_roundup(x, y)   ( ( ( (x) + ( (y) - 1 ) ) / (y) ) * (y) )
#define _PERL_HASH_SEED_roundup(x) __PERL_HASH_SEED_roundup(x,__PERL_HASH_WORD_SIZE)

#define PL_hash_seed ((U8 *)PL_hash_seed_w)
#define PL_hash_state ((U8 *)PL_hash_state_w)

#if PERL_HASH_USE_SBOX32_ALSO != 1
# define _PERL_HASH_FUNC                        __PERL_HASH_FUNC
# define _PERL_HASH_SEED_BYTES                  __PERL_HASH_SEED_BYTES
# define _PERL_HASH_STATE_BYTES                 __PERL_HASH_STATE_BYTES
# define _PERL_HASH_SEED_STATE(seed,state)      __PERL_HASH_SEED_STATE(seed,state)
# define _PERL_HASH_WITH_STATE(state,str,len)   __PERL_HASH_WITH_STATE(state,str,len)
#else

#define _PERL_HASH_FUNC         "SBOX32_WITH_" __PERL_HASH_FUNC
/* note the 4 in the below code comes from the fact the seed to initialize the SBOX is 128 bits */
#define _PERL_HASH_SEED_BYTES   ( __PERL_HASH_SEED_BYTES + (int)( 4 * sizeof(U32)) )

#define _PERL_HASH_STATE_BYTES  \
    ( __PERL_HASH_STATE_BYTES + ( ( 1 + ( 256 * SBOX32_MAX_LEN ) ) * sizeof(U32) ) )

#define _PERL_HASH_SEED_STATE(seed,state) STMT_START {                                      \
    __PERL_HASH_SEED_STATE(seed,state);                                                     \
    sbox32_seed_state128(seed + __PERL_HASH_SEED_BYTES, state + __PERL_HASH_STATE_BYTES);    \
} STMT_END

#define _PERL_HASH_WITH_STATE(state,str,len)                                            \
    (LIKELY(len <= SBOX32_MAX_LEN)                                                      \
        ? sbox32_hash_with_state((state + __PERL_HASH_STATE_BYTES),(U8*)(str),(len))    \
        : __PERL_HASH_WITH_STATE((state),(str),(len)))

#endif

#define PERL_HASH_WITH_SEED(seed,hash,str,len) \
    (hash) = S_perl_hash_with_seed((const U8 *) seed, (const U8 *) str,len)
#define PERL_HASH_WITH_STATE(state,hash,str,len) \
    (hash) = _PERL_HASH_WITH_STATE((state),(U8*)(str),(len))

#define PERL_HASH_SEED_STATE(seed,state) _PERL_HASH_SEED_STATE(seed,state)
#define PERL_HASH_SEED_BYTES _PERL_HASH_SEED_roundup(_PERL_HASH_SEED_BYTES)
#define PERL_HASH_STATE_BYTES _PERL_HASH_SEED_roundup(_PERL_HASH_STATE_BYTES)
#define PERL_HASH_FUNC        _PERL_HASH_FUNC

#define PERL_HASH_SEED_WORDS (PERL_HASH_SEED_BYTES/__PERL_HASH_WORD_SIZE)
#define PERL_HASH_STATE_WORDS (PERL_HASH_STATE_BYTES/__PERL_HASH_WORD_SIZE)

#ifdef PERL_USE_SINGLE_CHAR_HASH_CACHE
#define PERL_HASH(state,str,len) \
    (hash) = ((len) < 2 ? ( (len) == 0 ? PL_hash_chars[256] : PL_hash_chars[(U8)(str)[0]] ) \
                       : _PERL_HASH_WITH_STATE(PL_hash_state,(U8*)(str),(len)))
#else
#define PERL_HASH(hash,str,len) \
    PERL_HASH_WITH_STATE(PL_hash_state,hash,(U8*)(str),(len))
#endif

/* Setup the hash seed, either we do things dynamically at start up,
 * including reading from the environment, or we randomly setup the
 * seed. The seed will be passed into the PERL_HASH_SEED_STATE() function
 * defined for the configuration defined for this perl, which will then
 * initialize whatever state it might need later in hashing. */

#ifndef PERL_HASH_SEED
#   if defined(USE_HASH_SEED)
#       define PERL_HASH_SEED PL_hash_seed
#   else
       /* this is a 512 bit seed, which should be more than enough for the
        * configuration of any of our hash functions (with or without sbox).
        * If you actually use a hard coded seed, you are strongly encouraged
        * to replace this with something else of the correct length
        * for the hash function you are using (24-32 bytes depending on build
        * options). Repeat, you are *STRONGLY* encouraged not to use the value
        * provided here.
        */
#       define PERL_HASH_SEED \
           ((const U8 *)"A long string of pseudorandomly "  \
                        "chosen bytes for hashing in Perl")
#   endif
#endif

/* legacy - only mod_perl should be doing this.  */
#ifdef PERL_HASH_INTERNAL_ACCESS
#define PERL_HASH_INTERNAL(hash,str,len) PERL_HASH(hash,str,len)
#endif

PERL_STATIC_INLINE U32
S_perl_hash_with_seed(const U8 * seed, const U8 *str, STRLEN len) {
    __PERL_HASH_WORD_TYPE state[PERL_HASH_STATE_WORDS];
    _PERL_HASH_SEED_STATE(seed,(U8*)state);
    return _PERL_HASH_WITH_STATE((U8*)state,str,len);
}

#endif /*compile once*/

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
