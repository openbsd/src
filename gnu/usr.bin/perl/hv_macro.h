#ifndef PERL_SEEN_HV_MACRO_H /* compile once */
#define PERL_SEEN_HV_MACRO_H

#if IVSIZE == 8
#define CAN64BITHASH
#endif

/*-----------------------------------------------------------------------------
 * Endianess, misalignment capabilities and util macros
 *
 * The following 3 macros are defined in this section. The other macros defined
 * are only needed to help derive these 3.
 *
 * U8TO16_LE(x)   Read a little endian unsigned 32-bit int
 * U8TO32_LE(x)   Read a little endian unsigned 32-bit int
 * U8TO28_LE(x)   Read a little endian unsigned 32-bit int
 * ROTL32(x,r)      Rotate x left by r bits
 * ROTL64(x,r)      Rotate x left by r bits
 * ROTR32(x,r)      Rotate x right by r bits
 * ROTR64(x,r)      Rotate x right by r bits
 */

#ifndef U32_ALIGNMENT_REQUIRED
  #if (BYTEORDER == 0x1234 || BYTEORDER == 0x12345678)
    #define U8TO16_LE(ptr)   (*((const U16*)(ptr)))
    #define U8TO32_LE(ptr)   (*((const U32*)(ptr)))
    #define U8TO64_LE(ptr)   (*((const U64*)(ptr)))
  #elif (BYTEORDER == 0x4321 || BYTEORDER == 0x87654321)
    #if defined(__GNUC__) && (__GNUC__>4 || (__GNUC__==4 && __GNUC_MINOR__>=3))
      #define U8TO16_LE(ptr)   (__builtin_bswap16(*((U16*)(ptr))))
      #define U8TO32_LE(ptr)   (__builtin_bswap32(*((U32*)(ptr))))
      #define U8TO64_LE(ptr)   (__builtin_bswap64(*((U64*)(ptr))))
    #endif
  #endif
#endif

#ifndef U8TO16_LE
    /* Without a known fast bswap32 we're just as well off doing this */
  #define U8TO16_LE(ptr)   ((U32)(ptr)[0]|(U32)(ptr)[1]<<8)
  #define U8TO32_LE(ptr)   ((U32)(ptr)[0]|(U32)(ptr)[1]<<8|(U32)(ptr)[2]<<16|(U32)(ptr)[3]<<24)
  #define U8TO64_LE(ptr)   ((U64)(ptr)[0]|(U64)(ptr)[1]<<8|(U64)(ptr)[2]<<16|(U64)(ptr)[3]<<24|\
                            (U64)(ptr)[4]<<32|(U64)(ptr)[5]<<40|\
                            (U64)(ptr)[6]<<48|(U64)(ptr)[7]<<56)
#endif

#ifdef CAN64BITHASH
  #ifndef U64TYPE
  /* This probably isn't going to work, but failing with a compiler error due to
   lack of uint64_t is no worse than failing right now with an #error.  */
  #define U64 uint64_t
  #endif
#endif

/* Find best way to ROTL32/ROTL64 */
#if defined(_MSC_VER)
  #include <stdlib.h>  /* Microsoft put _rotl declaration in here */
  #define ROTL32(x,r)  _rotl(x,r)
  #define ROTR32(x,r)  _rotr(x,r)
  #define ROTL64(x,r)  _rotl64(x,r)
  #define ROTR64(x,r)  _rotr64(x,r)
#else
  /* gcc recognises this code and generates a rotate instruction for CPUs with one */
  #define ROTL32(x,r)  (((U32)(x) << (r)) | ((U32)(x) >> (32 - (r))))
  #define ROTR32(x,r)  (((U32)(x) << (32 - (r))) | ((U32)(x) >> (r)))
  #define ROTL64(x,r)  ( ( (U64)(x) << (r) ) | ( (U64)(x) >> ( 64 - (r) ) ) )
  #define ROTR64(x,r)  ( ( (U64)(x) << ( 64 - (r) ) ) | ( (U64)(x) >> (r) ) )
#endif


#ifdef UV_IS_QUAD
#define ROTL_UV(x,r) ROTL64(x,r)
#define ROTR_UV(x,r) ROTL64(x,r)
#else
#define ROTL_UV(x,r) ROTL32(x,r)
#define ROTR_UV(x,r) ROTR32(x,r)
#endif
#if IVSIZE == 8
#define CAN64BITHASH
#endif

#endif
