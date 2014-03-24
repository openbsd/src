/* hash a key
 *--------------------------------------------------------------------------------------
 * The "hash seed" feature was added in Perl 5.8.1 to perturb the results
 * to avoid "algorithmic complexity attacks".
 *
 * If USE_HASH_SEED is defined, hash randomisation is done by default
 * If USE_HASH_SEED_EXPLICIT is defined, hash randomisation is done
 * only if the environment variable PERL_HASH_SEED is set.
 * (see also perl.c:perl_parse() and S_init_tls_and_interp() and util.c:get_hash_seed())
 */

#ifndef PERL_SEEN_HV_FUNC_H /* compile once */
#define PERL_SEEN_HV_FUNC_H

#if !( 0 \
        || defined(PERL_HASH_FUNC_SIPHASH) \
        || defined(PERL_HASH_FUNC_SDBM) \
        || defined(PERL_HASH_FUNC_DJB2) \
        || defined(PERL_HASH_FUNC_SUPERFAST) \
        || defined(PERL_HASH_FUNC_MURMUR3) \
        || defined(PERL_HASH_FUNC_ONE_AT_A_TIME) \
        || defined(PERL_HASH_FUNC_ONE_AT_A_TIME_HARD) \
        || defined(PERL_HASH_FUNC_ONE_AT_A_TIME_OLD) \
    )
#define PERL_HASH_FUNC_ONE_AT_A_TIME_HARD
#endif

#if defined(PERL_HASH_FUNC_SIPHASH)
#   define PERL_HASH_FUNC "SIPHASH_2_4"
#   define PERL_HASH_SEED_BYTES 16
#   define PERL_HASH(hash,str,len) (hash)= S_perl_hash_siphash_2_4(PERL_HASH_SEED,(U8*)(str),(len))
#elif defined(PERL_HASH_FUNC_SUPERFAST)
#   define PERL_HASH_FUNC "SUPERFAST"
#   define PERL_HASH_SEED_BYTES 4
#   define PERL_HASH(hash,str,len) (hash)= S_perl_hash_superfast(PERL_HASH_SEED,(U8*)(str),(len))
#elif defined(PERL_HASH_FUNC_MURMUR3)
#   define PERL_HASH_FUNC "MURMUR3"
#   define PERL_HASH_SEED_BYTES 4
#   define PERL_HASH(hash,str,len) (hash)= S_perl_hash_murmur3(PERL_HASH_SEED,(U8*)(str),(len))
#elif defined(PERL_HASH_FUNC_DJB2)
#   define PERL_HASH_FUNC "DJB2"
#   define PERL_HASH_SEED_BYTES 4
#   define PERL_HASH(hash,str,len) (hash)= S_perl_hash_djb2(PERL_HASH_SEED,(U8*)(str),(len))
#elif defined(PERL_HASH_FUNC_SDBM)
#   define PERL_HASH_FUNC "SDBM"
#   define PERL_HASH_SEED_BYTES 4
#   define PERL_HASH(hash,str,len) (hash)= S_perl_hash_sdbm(PERL_HASH_SEED,(U8*)(str),(len))
#elif defined(PERL_HASH_FUNC_ONE_AT_A_TIME_HARD)
#   define PERL_HASH_FUNC "ONE_AT_A_TIME_HARD"
#   define PERL_HASH_SEED_BYTES 8
#   define PERL_HASH(hash,str,len) (hash)= S_perl_hash_one_at_a_time_hard(PERL_HASH_SEED,(U8*)(str),(len))
#elif defined(PERL_HASH_FUNC_ONE_AT_A_TIME)
#   define PERL_HASH_FUNC "ONE_AT_A_TIME"
#   define PERL_HASH_SEED_BYTES 4
#   define PERL_HASH(hash,str,len) (hash)= S_perl_hash_one_at_a_time(PERL_HASH_SEED,(U8*)(str),(len))
#elif defined(PERL_HASH_FUNC_ONE_AT_A_TIME_OLD)
#   define PERL_HASH_FUNC "ONE_AT_A_TIME_OLD"
#   define PERL_HASH_SEED_BYTES 4
#   define PERL_HASH(hash,str,len) (hash)= S_perl_hash_old_one_at_a_time(PERL_HASH_SEED,(U8*)(str),(len))
#endif

#ifndef PERL_HASH
#error "No hash function defined!"
#endif
#ifndef PERL_HASH_SEED_BYTES
#error "PERL_HASH_SEED_BYTES not defined"
#endif
#ifndef PERL_HASH_FUNC
#error "PERL_HASH_FUNC not defined"
#endif

#ifndef PERL_HASH_SEED
#   if defined(USE_HASH_SEED) || defined(USE_HASH_SEED_EXPLICIT)
#       define PERL_HASH_SEED PL_hash_seed
#   elif PERL_HASH_SEED_BYTES == 4
#       define PERL_HASH_SEED "PeRl"
#   elif PERL_HASH_SEED_BYTES == 16
#       define PERL_HASH_SEED "PeRlHaShhAcKpErl"
#   else
#       error "No PERL_HASH_SEED definition for " PERL_HASH_FUNC
#   endif
#endif

/*-----------------------------------------------------------------------------
 * Endianess, misalignment capabilities and util macros
 *
 * The following 3 macros are defined in this section. The other macros defined
 * are only needed to help derive these 3.
 *
 * U8TO32_LE(x)   Read a little endian unsigned 32-bit int
 * UNALIGNED_SAFE   Defined if READ_UINT32 works on non-word boundaries
 * ROTL32(x,r)      Rotate x left by r bits
 */

#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define U8TO16_LE(d) (*((const U16 *) (d)))
#endif

#if !defined (U8TO16_LE)
#define U8TO16_LE(d) ((((const U8 *)(d))[1] << 8)\
                      +((const U8 *)(d))[0])
#endif


/* Now find best way we can to READ_UINT32 */
#if (BYTEORDER == 0x1234 || BYTEORDER == 0x12345678) && U32SIZE == 4
  /* CPU endian matches murmurhash algorithm, so read 32-bit word directly */
  #define U8TO32_LE(ptr)   (*((U32*)(ptr)))
#elif BYTEORDER == 0x4321 || BYTEORDER == 0x87654321
  /* TODO: Add additional cases below where a compiler provided bswap32 is available */
  #if defined(__GNUC__) && (__GNUC__>4 || (__GNUC__==4 && __GNUC_MINOR__>=3))
    #define U8TO32_LE(ptr)   (__builtin_bswap32(*((U32*)(ptr))))
  #else
    /* Without a known fast bswap32 we're just as well off doing this */
    #define U8TO32_LE(ptr)   (ptr[0]|ptr[1]<<8|ptr[2]<<16|ptr[3]<<24)
    #define UNALIGNED_SAFE
  #endif
#else
  /* Unknown endianess so last resort is to read individual bytes */
  #define U8TO32_LE(ptr)   (ptr[0]|ptr[1]<<8|ptr[2]<<16|ptr[3]<<24)
  /* Since we're not doing word-reads we can skip the messing about with realignment */
  #define UNALIGNED_SAFE
#endif

#ifdef HAS_QUAD
#ifndef U64TYPE
/* This probably isn't going to work, but failing with a compiler error due to
   lack of uint64_t is no worse than failing right now with an #error.  */
#define U64TYPE uint64_t
#endif
#endif

/* Find best way to ROTL32/ROTL64 */
#if defined(_MSC_VER)
  #include <stdlib.h>  /* Microsoft put _rotl declaration in here */
  #define ROTL32(x,r)  _rotl(x,r)
  #ifdef HAS_QUAD
    #define ROTL64(x,r)  _rotl64(x,r)
  #endif
#else
  /* gcc recognises this code and generates a rotate instruction for CPUs with one */
  #define ROTL32(x,r)  (((U32)x << r) | ((U32)x >> (32 - r)))
  #ifdef HAS_QUAD
    #define ROTL64(x,r)  (((U64TYPE)x << r) | ((U64TYPE)x >> (64 - r)))
  #endif
#endif


#ifdef UV_IS_QUAD
#define ROTL_UV(x,r) ROTL64(x,r)
#else
#define ROTL_UV(x,r) ROTL32(x,r)
#endif

/* This is SipHash by Jean-Philippe Aumasson and Daniel J. Bernstein.
 * The authors claim it is relatively secure compared to the alternatives
 * and that performance wise it is a suitable hash for languages like Perl.
 * See:
 *
 * https://www.131002.net/siphash/
 *
 * This implementation seems to perform slightly slower than one-at-a-time for
 * short keys, but degrades slower for longer keys. Murmur Hash outperforms it
 * regardless of keys size.
 *
 * It is 64 bit only.
 */

#ifdef HAS_QUAD

#define U8TO64_LE(p) \
  (((U64TYPE)((p)[0])      ) | \
   ((U64TYPE)((p)[1]) <<  8) | \
   ((U64TYPE)((p)[2]) << 16) | \
   ((U64TYPE)((p)[3]) << 24) | \
   ((U64TYPE)((p)[4]) << 32) | \
   ((U64TYPE)((p)[5]) << 40) | \
   ((U64TYPE)((p)[6]) << 48) | \
   ((U64TYPE)((p)[7]) << 56))

#define SIPROUND            \
  do {              \
    v0 += v1; v1=ROTL64(v1,13); v1 ^= v0; v0=ROTL64(v0,32); \
    v2 += v3; v3=ROTL64(v3,16); v3 ^= v2;     \
    v0 += v3; v3=ROTL64(v3,21); v3 ^= v0;     \
    v2 += v1; v1=ROTL64(v1,17); v1 ^= v2; v2=ROTL64(v2,32); \
  } while(0)

/* SipHash-2-4 */

PERL_STATIC_INLINE U32
S_perl_hash_siphash_2_4(const unsigned char * const seed, const unsigned char *in, const STRLEN inlen) {
  /* "somepseudorandomlygeneratedbytes" */
  U64TYPE v0 = 0x736f6d6570736575ULL;
  U64TYPE v1 = 0x646f72616e646f6dULL;
  U64TYPE v2 = 0x6c7967656e657261ULL;
  U64TYPE v3 = 0x7465646279746573ULL;

  U64TYPE b;
  U64TYPE k0 = ((U64TYPE*)seed)[0];
  U64TYPE k1 = ((U64TYPE*)seed)[1];
  U64TYPE m;
  const int left = inlen & 7;
  const U8 *end = in + inlen - left;

  b = ( ( U64TYPE )(inlen) ) << 56;
  v3 ^= k1;
  v2 ^= k0;
  v1 ^= k1;
  v0 ^= k0;

  for ( ; in != end; in += 8 )
  {
    m = U8TO64_LE( in );
    v3 ^= m;
    SIPROUND;
    SIPROUND;
    v0 ^= m;
  }

  switch( left )
  {
  case 7: b |= ( ( U64TYPE )in[ 6] )  << 48;
  case 6: b |= ( ( U64TYPE )in[ 5] )  << 40;
  case 5: b |= ( ( U64TYPE )in[ 4] )  << 32;
  case 4: b |= ( ( U64TYPE )in[ 3] )  << 24;
  case 3: b |= ( ( U64TYPE )in[ 2] )  << 16;
  case 2: b |= ( ( U64TYPE )in[ 1] )  <<  8;
  case 1: b |= ( ( U64TYPE )in[ 0] ); break;
  case 0: break;
  }

  v3 ^= b;
  SIPROUND;
  SIPROUND;
  v0 ^= b;

  v2 ^= 0xff;
  SIPROUND;
  SIPROUND;
  SIPROUND;
  SIPROUND;
  b = v0 ^ v1 ^ v2  ^ v3;
  return (U32)(b & U32_MAX);
}
#endif /* defined(HAS_QUAD) */

/* FYI: This is the "Super-Fast" algorithm mentioned by Bob Jenkins in
 * (http://burtleburtle.net/bob/hash/doobs.html)
 * It is by Paul Hsieh (c) 2004 and is analysed here
 * http://www.azillionmonkeys.com/qed/hash.html
 * license terms are here:
 * http://www.azillionmonkeys.com/qed/weblicense.html
 */


PERL_STATIC_INLINE U32
S_perl_hash_superfast(const unsigned char * const seed, const unsigned char *str, STRLEN len) {
    U32 hash = *((U32*)seed) + len;
    U32 tmp;
    int rem= len & 3;
    len >>= 2;

    for (;len > 0; len--) {
        hash  += U8TO16_LE (str);
        tmp    = (U8TO16_LE (str+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        str   += 2 * sizeof (U16);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) { \
        case 3: hash += U8TO16_LE (str);
                hash ^= hash << 16;
                hash ^= str[sizeof (U16)] << 18;
                hash += hash >> 11;
                break;
        case 2: hash += U8TO16_LE (str);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += *str;
                hash ^= hash << 10;
                hash += hash >> 1;
    }
    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    return (hash + (hash >> 6));
}


/*-----------------------------------------------------------------------------
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain.
 *
 * This implementation was originally written by Shane Day, and is also public domain,
 * and was modified to function as a macro similar to other perl hash functions by
 * Yves Orton.
 *
 * This is a portable ANSI C implementation of MurmurHash3_x86_32 (Murmur3A)
 * with support for progressive processing.
 *
 * If you want to understand the MurmurHash algorithm you would be much better
 * off reading the original source. Just point your browser at:
 * http://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp
 *
 * How does it work?
 *
 * We can only process entire 32 bit chunks of input, except for the very end
 * that may be shorter.
 *
 * To handle endianess I simply use a macro that reads a U32 and define
 * that macro to be a direct read on little endian machines, a read and swap
 * on big endian machines, or a byte-by-byte read if the endianess is unknown.
 */


/*-----------------------------------------------------------------------------
 * Core murmurhash algorithm macros */

#define MURMUR_C1  (0xcc9e2d51)
#define MURMUR_C2  (0x1b873593)
#define MURMUR_C3  (0xe6546b64)
#define MURMUR_C4  (0x85ebca6b)
#define MURMUR_C5  (0xc2b2ae35)

/* This is the main processing body of the algorithm. It operates
 * on each full 32-bits of input. */
#define MURMUR_DOBLOCK(h1, k1) STMT_START { \
    k1 *= MURMUR_C1; \
    k1 = ROTL32(k1,15); \
    k1 *= MURMUR_C2; \
    \
    h1 ^= k1; \
    h1 = ROTL32(h1,13); \
    h1 = h1 * 5 + MURMUR_C3; \
} STMT_END


/* Append unaligned bytes to carry, forcing hash churn if we have 4 bytes */
/* cnt=bytes to process, h1=name of h1 var, c=carry, n=bytes in c, ptr/len=payload */
#define MURMUR_DOBYTES(cnt, h1, c, n, ptr, len) STMT_START { \
    int MURMUR_DOBYTES_i = cnt; \
    while(MURMUR_DOBYTES_i--) { \
        c = c>>8 | *ptr++<<24; \
        n++; len--; \
        if(n==4) { \
            MURMUR_DOBLOCK(h1, c); \
            n = 0; \
        } \
    } \
} STMT_END


/* now we create the hash function */
PERL_STATIC_INLINE U32
S_perl_hash_murmur3(const unsigned char * const seed, const unsigned char *ptr, STRLEN len) {
    U32 h1 = *((U32*)seed);
    U32 k1;
    U32 carry = 0;

    const unsigned char *end;
    int bytes_in_carry = 0; /* bytes in carry */
    I32 total_length= len;

#if defined(UNALIGNED_SAFE)
    /* Handle carry: commented out as its only used in incremental mode - it never fires for us
    int i = (4-n) & 3;
    if(i && i <= len) {
      MURMUR_DOBYTES(i, h1, carry, bytes_in_carry, ptr, len);
    }
    */

    /* This CPU handles unaligned word access */
    /* Process 32-bit chunks */
    end = ptr + len/4*4;
    for( ; ptr < end ; ptr+=4) {
        k1 = U8TO32_LE(ptr);
        MURMUR_DOBLOCK(h1, k1);
    }
#else
    /* This CPU does not handle unaligned word access */

    /* Consume enough so that the next data byte is word aligned */
    STRLEN i = -PTR2IV(ptr) & 3;
    if(i && i <= len) {
      MURMUR_DOBYTES(i, h1, carry, bytes_in_carry, ptr, len);
    }

    /* We're now aligned. Process in aligned blocks. Specialise for each possible carry count */
    end = ptr + len/4*4;
    switch(bytes_in_carry) { /* how many bytes in carry */
        case 0: /* c=[----]  w=[3210]  b=[3210]=w            c'=[----] */
        for( ; ptr < end ; ptr+=4) {
            k1 = U8TO32_LE(ptr);
            MURMUR_DOBLOCK(h1, k1);
        }
        break;
        case 1: /* c=[0---]  w=[4321]  b=[3210]=c>>24|w<<8   c'=[4---] */
        for( ; ptr < end ; ptr+=4) {
            k1 = carry>>24;
            carry = U8TO32_LE(ptr);
            k1 |= carry<<8;
            MURMUR_DOBLOCK(h1, k1);
        }
        break;
        case 2: /* c=[10--]  w=[5432]  b=[3210]=c>>16|w<<16  c'=[54--] */
        for( ; ptr < end ; ptr+=4) {
            k1 = carry>>16;
            carry = U8TO32_LE(ptr);
            k1 |= carry<<16;
            MURMUR_DOBLOCK(h1, k1);
        }
        break;
        case 3: /* c=[210-]  w=[6543]  b=[3210]=c>>8|w<<24   c'=[654-] */
        for( ; ptr < end ; ptr+=4) {
            k1 = carry>>8;
            carry = U8TO32_LE(ptr);
            k1 |= carry<<24;
            MURMUR_DOBLOCK(h1, k1);
        }
    }
#endif
    /* Advance over whole 32-bit chunks, possibly leaving 1..3 bytes */
    len -= len/4*4;

    /* Append any remaining bytes into carry */
    MURMUR_DOBYTES(len, h1, carry, bytes_in_carry, ptr, len);

    if (bytes_in_carry) {
        k1 = carry >> ( 4 - bytes_in_carry ) * 8;
        k1 *= MURMUR_C1;
        k1 = ROTL32(k1,15);
        k1 *= MURMUR_C2;
        h1 ^= k1;
    }
    h1 ^= total_length;

    /* fmix */
    h1 ^= h1 >> 16;
    h1 *= MURMUR_C4;
    h1 ^= h1 >> 13;
    h1 *= MURMUR_C5;
    h1 ^= h1 >> 16;
    return h1;
}


PERL_STATIC_INLINE U32
S_perl_hash_djb2(const unsigned char * const seed, const unsigned char *str, const STRLEN len) {
    const unsigned char * const end = (const unsigned char *)str + len;
    U32 hash = *((U32*)seed + len);
    while (str < end) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}

PERL_STATIC_INLINE U32
S_perl_hash_sdbm(const unsigned char * const seed, const unsigned char *str, const STRLEN len) {
    const unsigned char * const end = (const unsigned char *)str + len;
    U32 hash = *((U32*)seed + len);
    while (str < end) {
        hash = (hash << 6) + (hash << 16) - hash + *str++;
    }
    return hash;
}


/* This is the "One-at-a-Time" algorithm by Bob Jenkins
 * from requirements by Colin Plumb.
 * (http://burtleburtle.net/bob/hash/doobs.html)
 * With seed/len tweak.
 * */
PERL_STATIC_INLINE U32
S_perl_hash_one_at_a_time(const unsigned char * const seed, const unsigned char *str, const STRLEN len) {
    const unsigned char * const end = (const unsigned char *)str + len;
    U32 hash = *((U32*)seed) + len;
    while (str < end) {
        hash += *str++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    return (hash + (hash << 15));
}

/* Derived from "One-at-a-Time" algorithm by Bob Jenkins */
PERL_STATIC_INLINE U32
S_perl_hash_one_at_a_time_hard(const unsigned char * const seed, const unsigned char *str, const STRLEN len) {
    const unsigned char * const end = (const unsigned char *)str + len;
    U32 hash = *((U32*)seed) + len;
    
    while (str < end) {
        hash += (hash << 10);
        hash ^= (hash >> 6);
        hash += *str++;
    }
    
    hash += (hash << 10);
    hash ^= (hash >> 6);
    hash += seed[4];
    
    hash += (hash << 10);
    hash ^= (hash >> 6);
    hash += seed[5];
    
    hash += (hash << 10);
    hash ^= (hash >> 6);
    hash += seed[6];
    
    hash += (hash << 10);
    hash ^= (hash >> 6);
    hash += seed[7];
    
    hash += (hash << 10);
    hash ^= (hash >> 6);

    hash += (hash << 3);
    hash ^= (hash >> 11);
    return (hash + (hash << 15));
}

PERL_STATIC_INLINE U32
S_perl_hash_old_one_at_a_time(const unsigned char * const seed, const unsigned char *str, const STRLEN len) {
    const unsigned char * const end = (const unsigned char *)str + len;
    U32 hash = *((U32*)seed);
    while (str < end) {
        hash += *str++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    return (hash + (hash << 15));
}

/* legacy - only mod_perl should be doing this.  */
#ifdef PERL_HASH_INTERNAL_ACCESS
#define PERL_HASH_INTERNAL(hash,str,len) PERL_HASH(hash,str,len)
#endif

#endif /*compile once*/

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
