/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "rxkad_locl.h"


/*
 * this assumes that KRB_C_BIGENDIAN is used.
 * if we can find out endianess at compile-time, do so,
 * otherwise WORDS_BIGENDIAN should already have been defined
 */

#if ENDIANESS_IN_SYS_PARAM_H
#  undef WORDS_BIGENDIAN
#  include <sys/types.h>
#  include <sys/param.h>
#  if BYTE_ORDER == BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
#  elif BYTE_ORDER == LITTLE_ENDIAN
   /* ok */
#  else
#  error where do you cut your eggs?
#  endif
#endif

RCSID("$KTH: rxk_crpt.c,v 1.12 2000/10/03 00:38:27 lha Exp $");

/*
 * Unrolling of the inner loops helps the most on pentium chips
 * (ca 18%). On risc machines only expect a modest improvement (ca 5%).
 * The cost for this is rougly 4k bytes.
 */
#define UNROLL_LOOPS 1
/*
 * Inline assembler gives a boost only to fc_keysched.
 * On the pentium expect ca 28%.
 */
/*#define GNU_ASM 1 (now autoconfed) */

#if !defined(inline) && !defined(__GNUC__)
#define inline
#endif

#ifdef MANGLE_NAMES
#define fc_keysched    _afs_QTKrFdpoFL
#define fc_ecb_encrypt _afs_sDLThwNLok
#define fc_cbc_encrypt _afs_fkyCWTvfRS
#define rxkad_DecryptPacket _afs_SRWEeqTXrS
#define rxkad_EncryptPacket _afs_bpwQbdoghO
#endif

/*
 * There is usually no memcpy in kernels but gcc will inline all
 * calls to memcpy in this code anyway.
 */
#if defined(KERNEL) && !defined(__GNUC__)
#define memcpy(to, from, n) bcopy((from), (to), (n))
#endif

/* Rotate 32 bit word left */
#define ROT32L(x, n) ((((u_int32) x) << (n)) | (((u_int32) x) >> (32-(n))))
#define bswap32(x) (((ROT32L(x, 16) & 0x00ff00ff)<<8) | ((ROT32L(x, 16)>>8) & 0x00ff00ff))

#if WORDS_BIGENDIAN
#define NTOH(x) (x)
#else
#define NTOH(x) bswap32(x)
#endif

/*
 * Try to use a good function for ntohl-ing.
 *
 * The choice is done by autoconf setting EFF_NTOHL to one of:
 * CPU		function
 * i386		ntohl
 * i[4-9]86	bswap
 * alpha	bswap32
 * all else	ntohl
 */

#if defined(__GNUC__) && (defined(i386) || defined(__i386__))
static inline u_int32
bswap(u_int32 x)
{
  asm("bswap %0" : "=r" (x) : "0" (x));
  return x;
}
#endif

/*
 * Sboxes for Feistel network derived from
 * /afs/transarc.com/public/afsps/afs.rel31b.export-src/rxkad/sboxes.h
 */

#undef Z
#define Z(x) NTOH(x << 3)
static const u_int32 sbox0[256] = {
  Z(0xea), Z(0x7f), Z(0xb2), Z(0x64), Z(0x9d), Z(0xb0), Z(0xd9), Z(0x11), Z(0xcd), Z(0x86), Z(0x86),
  Z(0x91), Z(0x0a), Z(0xb2), Z(0x93), Z(0x06), Z(0x0e), Z(0x06), Z(0xd2), Z(0x65), Z(0x73), Z(0xc5),
  Z(0x28), Z(0x60), Z(0xf2), Z(0x20), Z(0xb5), Z(0x38), Z(0x7e), Z(0xda), Z(0x9f), Z(0xe3), Z(0xd2),
  Z(0xcf), Z(0xc4), Z(0x3c), Z(0x61), Z(0xff), Z(0x4a), Z(0x4a), Z(0x35), Z(0xac), Z(0xaa), Z(0x5f),
  Z(0x2b), Z(0xbb), Z(0xbc), Z(0x53), Z(0x4e), Z(0x9d), Z(0x78), Z(0xa3), Z(0xdc), Z(0x09), Z(0x32),
  Z(0x10), Z(0xc6), Z(0x6f), Z(0x66), Z(0xd6), Z(0xab), Z(0xa9), Z(0xaf), Z(0xfd), Z(0x3b), Z(0x95),
  Z(0xe8), Z(0x34), Z(0x9a), Z(0x81), Z(0x72), Z(0x80), Z(0x9c), Z(0xf3), Z(0xec), Z(0xda), Z(0x9f),
  Z(0x26), Z(0x76), Z(0x15), Z(0x3e), Z(0x55), Z(0x4d), Z(0xde), Z(0x84), Z(0xee), Z(0xad), Z(0xc7),
  Z(0xf1), Z(0x6b), Z(0x3d), Z(0xd3), Z(0x04), Z(0x49), Z(0xaa), Z(0x24), Z(0x0b), Z(0x8a), Z(0x83),
  Z(0xba), Z(0xfa), Z(0x85), Z(0xa0), Z(0xa8), Z(0xb1), Z(0xd4), Z(0x01), Z(0xd8), Z(0x70), Z(0x64),
  Z(0xf0), Z(0x51), Z(0xd2), Z(0xc3), Z(0xa7), Z(0x75), Z(0x8c), Z(0xa5), Z(0x64), Z(0xef), Z(0x10),
  Z(0x4e), Z(0xb7), Z(0xc6), Z(0x61), Z(0x03), Z(0xeb), Z(0x44), Z(0x3d), Z(0xe5), Z(0xb3), Z(0x5b),
  Z(0xae), Z(0xd5), Z(0xad), Z(0x1d), Z(0xfa), Z(0x5a), Z(0x1e), Z(0x33), Z(0xab), Z(0x93), Z(0xa2),
  Z(0xb7), Z(0xe7), Z(0xa8), Z(0x45), Z(0xa4), Z(0xcd), Z(0x29), Z(0x63), Z(0x44), Z(0xb6), Z(0x69),
  Z(0x7e), Z(0x2e), Z(0x62), Z(0x03), Z(0xc8), Z(0xe0), Z(0x17), Z(0xbb), Z(0xc7), Z(0xf3), Z(0x3f),
  Z(0x36), Z(0xba), Z(0x71), Z(0x8e), Z(0x97), Z(0x65), Z(0x60), Z(0x69), Z(0xb6), Z(0xf6), Z(0xe6),
  Z(0x6e), Z(0xe0), Z(0x81), Z(0x59), Z(0xe8), Z(0xaf), Z(0xdd), Z(0x95), Z(0x22), Z(0x99), Z(0xfd),
  Z(0x63), Z(0x19), Z(0x74), Z(0x61), Z(0xb1), Z(0xb6), Z(0x5b), Z(0xae), Z(0x54), Z(0xb3), Z(0x70),
  Z(0xff), Z(0xc6), Z(0x3b), Z(0x3e), Z(0xc1), Z(0xd7), Z(0xe1), Z(0x0e), Z(0x76), Z(0xe5), Z(0x36),
  Z(0x4f), Z(0x59), Z(0xc7), Z(0x08), Z(0x6e), Z(0x82), Z(0xa6), Z(0x93), Z(0xc4), Z(0xaa), Z(0x26),
  Z(0x49), Z(0xe0), Z(0x21), Z(0x64), Z(0x07), Z(0x9f), Z(0x64), Z(0x81), Z(0x9c), Z(0xbf), Z(0xf9),
  Z(0xd1), Z(0x43), Z(0xf8), Z(0xb6), Z(0xb9), Z(0xf1), Z(0x24), Z(0x75), Z(0x03), Z(0xe4), Z(0xb0),
  Z(0x99), Z(0x46), Z(0x3d), Z(0xf5), Z(0xd1), Z(0x39), Z(0x72), Z(0x12), Z(0xf6), Z(0xba), Z(0x0c),
  Z(0x0d), Z(0x42), Z(0x2e)};

#undef Z
#define Z(x) NTOH((x << 27) | (x >> 5))
static const u_int32 sbox1[256] = {
  Z(0x77), Z(0x14), Z(0xa6), Z(0xfe), Z(0xb2), Z(0x5e), Z(0x8c), Z(0x3e), Z(0x67), Z(0x6c), Z(0xa1),
  Z(0x0d), Z(0xc2), Z(0xa2), Z(0xc1), Z(0x85), Z(0x6c), Z(0x7b), Z(0x67), Z(0xc6), Z(0x23), Z(0xe3),
  Z(0xf2), Z(0x89), Z(0x50), Z(0x9c), Z(0x03), Z(0xb7), Z(0x73), Z(0xe6), Z(0xe1), Z(0x39), Z(0x31),
  Z(0x2c), Z(0x27), Z(0x9f), Z(0xa5), Z(0x69), Z(0x44), Z(0xd6), Z(0x23), Z(0x83), Z(0x98), Z(0x7d),
  Z(0x3c), Z(0xb4), Z(0x2d), Z(0x99), Z(0x1c), Z(0x1f), Z(0x8c), Z(0x20), Z(0x03), Z(0x7c), Z(0x5f),
  Z(0xad), Z(0xf4), Z(0xfa), Z(0x95), Z(0xca), Z(0x76), Z(0x44), Z(0xcd), Z(0xb6), Z(0xb8), Z(0xa1),
  Z(0xa1), Z(0xbe), Z(0x9e), Z(0x54), Z(0x8f), Z(0x0b), Z(0x16), Z(0x74), Z(0x31), Z(0x8a), Z(0x23),
  Z(0x17), Z(0x04), Z(0xfa), Z(0x79), Z(0x84), Z(0xb1), Z(0xf5), Z(0x13), Z(0xab), Z(0xb5), Z(0x2e),
  Z(0xaa), Z(0x0c), Z(0x60), Z(0x6b), Z(0x5b), Z(0xc4), Z(0x4b), Z(0xbc), Z(0xe2), Z(0xaf), Z(0x45),
  Z(0x73), Z(0xfa), Z(0xc9), Z(0x49), Z(0xcd), Z(0x00), Z(0x92), Z(0x7d), Z(0x97), Z(0x7a), Z(0x18),
  Z(0x60), Z(0x3d), Z(0xcf), Z(0x5b), Z(0xde), Z(0xc6), Z(0xe2), Z(0xe6), Z(0xbb), Z(0x8b), Z(0x06),
  Z(0xda), Z(0x08), Z(0x15), Z(0x1b), Z(0x88), Z(0x6a), Z(0x17), Z(0x89), Z(0xd0), Z(0xa9), Z(0xc1),
  Z(0xc9), Z(0x70), Z(0x6b), Z(0xe5), Z(0x43), Z(0xf4), Z(0x68), Z(0xc8), Z(0xd3), Z(0x84), Z(0x28),
  Z(0x0a), Z(0x52), Z(0x66), Z(0xa3), Z(0xca), Z(0xf2), Z(0xe3), Z(0x7f), Z(0x7a), Z(0x31), Z(0xf7),
  Z(0x88), Z(0x94), Z(0x5e), Z(0x9c), Z(0x63), Z(0xd5), Z(0x24), Z(0x66), Z(0xfc), Z(0xb3), Z(0x57),
  Z(0x25), Z(0xbe), Z(0x89), Z(0x44), Z(0xc4), Z(0xe0), Z(0x8f), Z(0x23), Z(0x3c), Z(0x12), Z(0x52),
  Z(0xf5), Z(0x1e), Z(0xf4), Z(0xcb), Z(0x18), Z(0x33), Z(0x1f), Z(0xf8), Z(0x69), Z(0x10), Z(0x9d),
  Z(0xd3), Z(0xf7), Z(0x28), Z(0xf8), Z(0x30), Z(0x05), Z(0x5e), Z(0x32), Z(0xc0), Z(0xd5), Z(0x19),
  Z(0xbd), Z(0x45), Z(0x8b), Z(0x5b), Z(0xfd), Z(0xbc), Z(0xe2), Z(0x5c), Z(0xa9), Z(0x96), Z(0xef),
  Z(0x70), Z(0xcf), Z(0xc2), Z(0x2a), Z(0xb3), Z(0x61), Z(0xad), Z(0x80), Z(0x48), Z(0x81), Z(0xb7),
  Z(0x1d), Z(0x43), Z(0xd9), Z(0xd7), Z(0x45), Z(0xf0), Z(0xd8), Z(0x8a), Z(0x59), Z(0x7c), Z(0x57),
  Z(0xc1), Z(0x79), Z(0xc7), Z(0x34), Z(0xd6), Z(0x43), Z(0xdf), Z(0xe4), Z(0x78), Z(0x16), Z(0x06),
  Z(0xda), Z(0x92), Z(0x76), Z(0x51), Z(0xe1), Z(0xd4), Z(0x70), Z(0x03), Z(0xe0), Z(0x2f), Z(0x96),
  Z(0x91), Z(0x82), Z(0x80)};

#undef Z
#define Z(x) NTOH(x << 11)
static const u_int32 sbox2[256] = {
  Z(0xf0), Z(0x37), Z(0x24), Z(0x53), Z(0x2a), Z(0x03), Z(0x83), Z(0x86), Z(0xd1), Z(0xec), Z(0x50),
  Z(0xf0), Z(0x42), Z(0x78), Z(0x2f), Z(0x6d), Z(0xbf), Z(0x80), Z(0x87), Z(0x27), Z(0x95), Z(0xe2),
  Z(0xc5), Z(0x5d), Z(0xf9), Z(0x6f), Z(0xdb), Z(0xb4), Z(0x65), Z(0x6e), Z(0xe7), Z(0x24), Z(0xc8),
  Z(0x1a), Z(0xbb), Z(0x49), Z(0xb5), Z(0x0a), Z(0x7d), Z(0xb9), Z(0xe8), Z(0xdc), Z(0xb7), Z(0xd9),
  Z(0x45), Z(0x20), Z(0x1b), Z(0xce), Z(0x59), Z(0x9d), Z(0x6b), Z(0xbd), Z(0x0e), Z(0x8f), Z(0xa3),
  Z(0xa9), Z(0xbc), Z(0x74), Z(0xa6), Z(0xf6), Z(0x7f), Z(0x5f), Z(0xb1), Z(0x68), Z(0x84), Z(0xbc),
  Z(0xa9), Z(0xfd), Z(0x55), Z(0x50), Z(0xe9), Z(0xb6), Z(0x13), Z(0x5e), Z(0x07), Z(0xb8), Z(0x95),
  Z(0x02), Z(0xc0), Z(0xd0), Z(0x6a), Z(0x1a), Z(0x85), Z(0xbd), Z(0xb6), Z(0xfd), Z(0xfe), Z(0x17),
  Z(0x3f), Z(0x09), Z(0xa3), Z(0x8d), Z(0xfb), Z(0xed), Z(0xda), Z(0x1d), Z(0x6d), Z(0x1c), Z(0x6c),
  Z(0x01), Z(0x5a), Z(0xe5), Z(0x71), Z(0x3e), Z(0x8b), Z(0x6b), Z(0xbe), Z(0x29), Z(0xeb), Z(0x12),
  Z(0x19), Z(0x34), Z(0xcd), Z(0xb3), Z(0xbd), Z(0x35), Z(0xea), Z(0x4b), Z(0xd5), Z(0xae), Z(0x2a),
  Z(0x79), Z(0x5a), Z(0xa5), Z(0x32), Z(0x12), Z(0x7b), Z(0xdc), Z(0x2c), Z(0xd0), Z(0x22), Z(0x4b),
  Z(0xb1), Z(0x85), Z(0x59), Z(0x80), Z(0xc0), Z(0x30), Z(0x9f), Z(0x73), Z(0xd3), Z(0x14), Z(0x48),
  Z(0x40), Z(0x07), Z(0x2d), Z(0x8f), Z(0x80), Z(0x0f), Z(0xce), Z(0x0b), Z(0x5e), Z(0xb7), Z(0x5e),
  Z(0xac), Z(0x24), Z(0x94), Z(0x4a), Z(0x18), Z(0x15), Z(0x05), Z(0xe8), Z(0x02), Z(0x77), Z(0xa9),
  Z(0xc7), Z(0x40), Z(0x45), Z(0x89), Z(0xd1), Z(0xea), Z(0xde), Z(0x0c), Z(0x79), Z(0x2a), Z(0x99),
  Z(0x6c), Z(0x3e), Z(0x95), Z(0xdd), Z(0x8c), Z(0x7d), Z(0xad), Z(0x6f), Z(0xdc), Z(0xff), Z(0xfd),
  Z(0x62), Z(0x47), Z(0xb3), Z(0x21), Z(0x8a), Z(0xec), Z(0x8e), Z(0x19), Z(0x18), Z(0xb4), Z(0x6e),
  Z(0x3d), Z(0xfd), Z(0x74), Z(0x54), Z(0x1e), Z(0x04), Z(0x85), Z(0xd8), Z(0xbc), Z(0x1f), Z(0x56),
  Z(0xe7), Z(0x3a), Z(0x56), Z(0x67), Z(0xd6), Z(0xc8), Z(0xa5), Z(0xf3), Z(0x8e), Z(0xde), Z(0xae),
  Z(0x37), Z(0x49), Z(0xb7), Z(0xfa), Z(0xc8), Z(0xf4), Z(0x1f), Z(0xe0), Z(0x2a), Z(0x9b), Z(0x15),
  Z(0xd1), Z(0x34), Z(0x0e), Z(0xb5), Z(0xe0), Z(0x44), Z(0x78), Z(0x84), Z(0x59), Z(0x56), Z(0x68),
  Z(0x77), Z(0xa5), Z(0x14), Z(0x06), Z(0xf5), Z(0x2f), Z(0x8c), Z(0x8a), Z(0x73), Z(0x80), Z(0x76),
  Z(0xb4), Z(0x10), Z(0x86)};

#undef Z
#define Z(x) NTOH(x << 19)
static const u_int32 sbox3[256] = {
  Z(0xa9), Z(0x2a), Z(0x48), Z(0x51), Z(0x84), Z(0x7e), Z(0x49), Z(0xe2), Z(0xb5), Z(0xb7), Z(0x42),
  Z(0x33), Z(0x7d), Z(0x5d), Z(0xa6), Z(0x12), Z(0x44), Z(0x48), Z(0x6d), Z(0x28), Z(0xaa), Z(0x20),
  Z(0x6d), Z(0x57), Z(0xd6), Z(0x6b), Z(0x5d), Z(0x72), Z(0xf0), Z(0x92), Z(0x5a), Z(0x1b), Z(0x53),
  Z(0x80), Z(0x24), Z(0x70), Z(0x9a), Z(0xcc), Z(0xa7), Z(0x66), Z(0xa1), Z(0x01), Z(0xa5), Z(0x41),
  Z(0x97), Z(0x41), Z(0x31), Z(0x82), Z(0xf1), Z(0x14), Z(0xcf), Z(0x53), Z(0x0d), Z(0xa0), Z(0x10),
  Z(0xcc), Z(0x2a), Z(0x7d), Z(0xd2), Z(0xbf), Z(0x4b), Z(0x1a), Z(0xdb), Z(0x16), Z(0x47), Z(0xf6),
  Z(0x51), Z(0x36), Z(0xed), Z(0xf3), Z(0xb9), Z(0x1a), Z(0xa7), Z(0xdf), Z(0x29), Z(0x43), Z(0x01),
  Z(0x54), Z(0x70), Z(0xa4), Z(0xbf), Z(0xd4), Z(0x0b), Z(0x53), Z(0x44), Z(0x60), Z(0x9e), Z(0x23),
  Z(0xa1), Z(0x18), Z(0x68), Z(0x4f), Z(0xf0), Z(0x2f), Z(0x82), Z(0xc2), Z(0x2a), Z(0x41), Z(0xb2),
  Z(0x42), Z(0x0c), Z(0xed), Z(0x0c), Z(0x1d), Z(0x13), Z(0x3a), Z(0x3c), Z(0x6e), Z(0x35), Z(0xdc),
  Z(0x60), Z(0x65), Z(0x85), Z(0xe9), Z(0x64), Z(0x02), Z(0x9a), Z(0x3f), Z(0x9f), Z(0x87), Z(0x96),
  Z(0xdf), Z(0xbe), Z(0xf2), Z(0xcb), Z(0xe5), Z(0x6c), Z(0xd4), Z(0x5a), Z(0x83), Z(0xbf), Z(0x92),
  Z(0x1b), Z(0x94), Z(0x00), Z(0x42), Z(0xcf), Z(0x4b), Z(0x00), Z(0x75), Z(0xba), Z(0x8f), Z(0x76),
  Z(0x5f), Z(0x5d), Z(0x3a), Z(0x4d), Z(0x09), Z(0x12), Z(0x08), Z(0x38), Z(0x95), Z(0x17), Z(0xe4),
  Z(0x01), Z(0x1d), Z(0x4c), Z(0xa9), Z(0xcc), Z(0x85), Z(0x82), Z(0x4c), Z(0x9d), Z(0x2f), Z(0x3b),
  Z(0x66), Z(0xa1), Z(0x34), Z(0x10), Z(0xcd), Z(0x59), Z(0x89), Z(0xa5), Z(0x31), Z(0xcf), Z(0x05),
  Z(0xc8), Z(0x84), Z(0xfa), Z(0xc7), Z(0xba), Z(0x4e), Z(0x8b), Z(0x1a), Z(0x19), Z(0xf1), Z(0xa1),
  Z(0x3b), Z(0x18), Z(0x12), Z(0x17), Z(0xb0), Z(0x98), Z(0x8d), Z(0x0b), Z(0x23), Z(0xc3), Z(0x3a),
  Z(0x2d), Z(0x20), Z(0xdf), Z(0x13), Z(0xa0), Z(0xa8), Z(0x4c), Z(0x0d), Z(0x6c), Z(0x2f), Z(0x47),
  Z(0x13), Z(0x13), Z(0x52), Z(0x1f), Z(0x2d), Z(0xf5), Z(0x79), Z(0x3d), Z(0xa2), Z(0x54), Z(0xbd),
  Z(0x69), Z(0xc8), Z(0x6b), Z(0xf3), Z(0x05), Z(0x28), Z(0xf1), Z(0x16), Z(0x46), Z(0x40), Z(0xb0),
  Z(0x11), Z(0xd3), Z(0xb7), Z(0x95), Z(0x49), Z(0xcf), Z(0xc3), Z(0x1d), Z(0x8f), Z(0xd8), Z(0xe1),
  Z(0x73), Z(0xdb), Z(0xad), Z(0xc8), Z(0xc9), Z(0xa9), Z(0xa1), Z(0xc2), Z(0xc5), Z(0xe3), Z(0xba),
  Z(0xfc), Z(0x0e), Z(0x25)};

/*
 * This is a 16 round Feistel network with permutation F_ENCRYPT
 */

#define F_ENCRYPT(R, L, sched) { \
 union lc4 { u_int32 l; unsigned char c[4]; } u; \
 u.l = sched ^ R; \
 L ^= sbox0[u.c[0]] ^ sbox1[u.c[1]] ^ sbox2[u.c[2]] ^ sbox3[u.c[3]]; }

#ifndef WORDS_BIGENDIAN
/* BEWARE: this code is endian dependent.
 * This should really be inline assembler on the x86.
 */
#undef F_ENCRYPT
#define FF(y, shiftN) (((y) >> shiftN) & 0xFF)
#define F_ENCRYPT(R, L, sched) { \
 u_int32 u; \
 u = sched ^ R; \
 L ^= sbox0[FF(u, 0)] ^ sbox1[FF(u, 8)] ^ sbox2[FF(u, 16)] ^ sbox3[FF(u, 24)];}
#endif

static inline
void
fc_ecb_enc(u_int32 l,
	   u_int32 r,
	   u_int32 out[2],
	   const int32 sched[ROUNDS])
{
#if !defined(UNROLL_LOOPS)
  {
    int i;
    for (i = 0; i < (ROUNDS/4); i++)
      {
	F_ENCRYPT(r, l, *sched++);
	F_ENCRYPT(l, r, *sched++);
	F_ENCRYPT(r, l, *sched++);
	F_ENCRYPT(l, r, *sched++);
      }
  }
#else
  F_ENCRYPT(r, l, *sched++);
  F_ENCRYPT(l, r, *sched++);
  F_ENCRYPT(r, l, *sched++);
  F_ENCRYPT(l, r, *sched++);
  F_ENCRYPT(r, l, *sched++);
  F_ENCRYPT(l, r, *sched++);
  F_ENCRYPT(r, l, *sched++);
  F_ENCRYPT(l, r, *sched++);
  F_ENCRYPT(r, l, *sched++);
  F_ENCRYPT(l, r, *sched++);
  F_ENCRYPT(r, l, *sched++);
  F_ENCRYPT(l, r, *sched++);
  F_ENCRYPT(r, l, *sched++);
  F_ENCRYPT(l, r, *sched++);
  F_ENCRYPT(r, l, *sched++);
  F_ENCRYPT(l, r, *sched++);
#endif /* UNROLL_LOOPS */

  out[0] = l;
  out[1] = r;
}

static inline
void
fc_ecb_dec(u_int32 l,
	   u_int32 r,
	   u_int32 out[2],
	   const int32 sched[ROUNDS])
{
  sched = &sched[ROUNDS-1];

#if !defined(UNROLL_LOOPS)
  {
    int i;
    for (i = 0; i < (ROUNDS/4); i++)
      {
	F_ENCRYPT(l, r, *sched--);
	F_ENCRYPT(r, l, *sched--);
	F_ENCRYPT(l, r, *sched--);
	F_ENCRYPT(r, l, *sched--);
      }
  }
#else
  F_ENCRYPT(l, r, *sched--);
  F_ENCRYPT(r, l, *sched--);
  F_ENCRYPT(l, r, *sched--);
  F_ENCRYPT(r, l, *sched--);
  F_ENCRYPT(l, r, *sched--);
  F_ENCRYPT(r, l, *sched--);
  F_ENCRYPT(l, r, *sched--);
  F_ENCRYPT(r, l, *sched--);
  F_ENCRYPT(l, r, *sched--);
  F_ENCRYPT(r, l, *sched--);
  F_ENCRYPT(l, r, *sched--);
  F_ENCRYPT(r, l, *sched--);
  F_ENCRYPT(l, r, *sched--);
  F_ENCRYPT(r, l, *sched--);
  F_ENCRYPT(l, r, *sched--);
  F_ENCRYPT(r, l, *sched--);
#endif /* UNROLL_LOOPS */

  out[0] = l;
  out[1] = r;
}

static inline
void
fc_cbc_enc(const u_int32 *in,
	   u_int32 *out,
	   int32 length,
	   const int32 sched[ROUNDS],
	   u_int32 iv[2])
{
  int32 xor0 = iv[0], xor1 = iv[1];

  for (; length > 0; length -= 8)
    {
      u_int32 b8[2];
      /* If length < 8 we read to much, usally ok */
      xor0 ^= in[0];
      xor1 ^= in[1];
      fc_ecb_enc(xor0, xor1, b8, sched);
      xor0 = in[0] ^ b8[0];
      xor1 = in[1] ^ b8[1];

      /* Out is always a multiple of 8 */
      memcpy(out, b8, 8);
      out += 2;
      in += 2;
    }
  iv[0] = xor0;
  iv[1] = xor1;
}

static inline
void
fc_cbc_dec(const u_int32 *in,
	   u_int32 *out,
	   int32 length,
	   const int32 sched[ROUNDS],
	   u_int32 iv[2])
{
  int32 xor0 = iv[0], xor1 = iv[1];

  for (; length > 0; length -= 8)
    {
      u_int32 b8[2];
      /* In is always a multiple of 8 */
      fc_ecb_dec(in[0], in[1], b8, sched);
      b8[0] ^= xor0;
      b8[1] ^= xor1;
      xor0 = in[0] ^ b8[0];
      xor1 = in[1] ^ b8[1];

#if 0
      if (length >= 8)
	memcpy(out, b8, 8);
      else
	memcpy(out, b8, length); /* Don't write to much when length < 8 */
#else
      /* If length < 8 we write to much, this is not always ok */
      memcpy(out, b8, 8);
#endif
      out += 2;
      in += 2;
    }
  iv[0] = xor0;
  iv[1] = xor1;
}

int
fc_ecb_encrypt(const void *in_,
	       void *out_,
	       const int32 sched[ROUNDS],
	       int encrypt)
{
  const u_int32 *in = in_;	/*  In must be u_int32 aligned */
  u_int32 *out = out_;		/* Out must be u_int32 aligned */
  if (encrypt)
    fc_ecb_enc(in[0], in[1], out, sched);
  else
    fc_ecb_dec(in[0], in[1], out, sched);
  return 0;
}

int
fc_cbc_encrypt(const void *in_,
	       void *out_,
	       int32 length,
	       const int32 sched[ROUNDS],
	       u_int32 iv[2],
	       int encrypt)
{
  const u_int32 *in = in_;	/*  In must be u_int32 aligned */
  u_int32 *out = out_;		/* Out must be u_int32 aligned */
  if (encrypt)
    fc_cbc_enc(in, out, length, sched, iv);
  else
    fc_cbc_dec(in, out, length, sched, iv);
  return 0;
}

/* Rotate two 32 bit numbers as a 56 bit number */
#define ROT56R(hi, lo, n) { \
  u_int32 t = lo & ((1<<n)-1); \
  lo = (lo >> n) | ((hi & ((1<<n)-1)) << (32-n)); \
  hi = (hi >> n) | (t << (24-n)); }

/* Rotate one 64 bit number as a 56 bit number */
#define ROT56R64(k, n) { \
  k = (k >> n) | ((k & ((1<<n) - 1)) << (56-n)); }

/*
 * Generate a key schedule from key, the least significant bit in each
 * key byte is parity and shall be ignored. This leaves 56 significant
 * bits in the key to scatter over the 16 key schedules. For each
 * schedule extract the low order 32 bits and use as schedule, then
 * rotate right by 11 bits.
 *
 * Note that this fc_keysched() generates a schedule in natural byte
 * order, the Transarc function does not. Therefore it's *not*
 * possible to mix fc_keysched, fc_ecb_encrypt and fc_cbc_encrypt
 * from different implementations. Keep them in the same module!
 */
int
fc_keysched(const void *key_,
	    int32 sched[ROUNDS])
{
  const unsigned char *key = key_;

  /* Do we have 56 bit longs or even longer longs? */
#if ((1ul << 31) << 1) && defined(ULONG_MAX) && ((ULONG_MAX >> 55) != 0) && ((1ul << 55) != 0)
  unsigned long k;		/* k holds all 56 non parity bits */

  /* Compress out parity bits */
  k = (*key++) >> 1;
  k <<= 7;
  k |= (*key++) >> 1;
  k <<= 7;
  k |= (*key++) >> 1;
  k <<= 7;
  k |= (*key++) >> 1;
  k <<= 7;
  k |= (*key++) >> 1;
  k <<= 7;
  k |= (*key++) >> 1;
  k <<= 7;
  k |= (*key++) >> 1;
  k <<= 7;
  k |= (*key) >> 1;

  /* Use lower 32 bits for schedule, rotate by 11 each round (16 times) */
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);

  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);

  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);

  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  ROT56R64(k, 11);
  *sched++ = EFF_NTOHL((u_int32)k);
  return 0;
#else
  u_int32 hi, lo; /* hi is upper 24 bits and lo lower 32, total 56 */

  /* Compress out parity bits */
  lo = (*key++) >> 1;
  lo <<= 7;
  lo |= (*key++) >> 1;
  lo <<= 7;
  lo |= (*key++) >> 1;
  lo <<= 7;
  lo |= (*key++) >> 1;
  hi = lo >> 4;
  lo &= 0xf;
  lo <<= 7;
  lo |= (*key++) >> 1;
  lo <<= 7;
  lo |= (*key++) >> 1;
  lo <<= 7;
  lo |= (*key++) >> 1;
  lo <<= 7;
  lo |= (*key) >> 1;

  /* Use lower 32 bits for schedule, rotate by 11 each round (16 times) */
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);

  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);

  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);

  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  ROT56R(hi, lo, 11);
  *sched++ = EFF_NTOHL(lo);
  return 0;
#endif
}

/*
 * Encryption/decryption of Rx packets is pretty straight forward. Run
 * fc_cbc_encrypt over the packet fragments until len bytes have been
 * processed. Skip the Rx packet header but not the security header.
 */
int
rxkad_EncryptPacket(const void *rx_connection_not_used,
		    const int32 sched[ROUNDS],
		    const u_int32 iv[2],
		    int len,
		    struct rx_packet *packet)
{
  u_int32 ivec[2];
  struct iovec *frag;

  {
    /* What is this good for?
     * It turns out that the security header for auth_enc is of
     * size 8 bytes and the last 4 bytes are defined to be 0!
     */
    u_int32 *t = (u_int32 *)packet->wirevec[1].iov_base;
    t[1] = 0;
  }

  memcpy(ivec, iv, sizeof(ivec)); /* Must use copy of iv */
  for (frag = &packet->wirevec[1]; len; frag++)
    {
      int      iov_len = frag->iov_len;
      u_int32 *iov_bas = (u_int32 *) frag->iov_base;
      if (iov_len == 0)
	return RXKADDATALEN;	/* Length mismatch */
      if (len < iov_len)
	iov_len = len;		/* Don't process to much data */
      fc_cbc_enc(iov_bas, iov_bas, iov_len, sched, ivec);
      len -= iov_len;
    }
  return 0;
}

int
rxkad_DecryptPacket(const void *rx_connection_not_used,
		    const int32 sched[ROUNDS],
		    const u_int32 iv[2],
		    int len,
		    struct rx_packet *packet)
{
  u_int32 ivec[2];
  struct iovec *frag;

  memcpy(ivec, iv, sizeof(ivec)); /* Must use copy of iv */
  for (frag = &packet->wirevec[1]; len > 0; frag++)
    {
      int      iov_len = frag->iov_len;
      u_int32 *iov_bas = (u_int32 *) frag->iov_base;
      if (iov_len == 0)
	return RXKADDATALEN;	/* Length mismatch */
      if (len < iov_len)
	iov_len = len;		/* Don't process to much data */
      fc_cbc_dec(iov_bas, iov_bas, iov_len, sched, ivec);
      len -= iov_len;
    }
  return 0;
}

#if defined(TEST) || defined(TEST_KERNEL)
/*
 * It is possible to link with the client kernel libafs.a to verify
 * the test case. Use TEST_KERNEL to get the mangled names.
 */

#include <stdio.h>
#include <string.h>

#include <time.h>

const char the_quick[] = "The quick brown fox jumps over the lazy dogs.\0\0";

const unsigned char key1[8]={0xf0,0xe1,0xd2,0xc3,0xb4,0xa5,0x96,0x87};
const char ciph1[] = {
  0x00, 0xf0, 0xe,  0x11, 0x75, 0xe6, 0x23, 0x82, 0xee, 0xac, 0x98, 0x62,
  0x44, 0x51, 0xe4, 0x84, 0xc3, 0x59, 0xd8, 0xaa, 0x64, 0x60, 0xae, 0xf7,
  0xd2, 0xd9, 0x13, 0x79, 0x72, 0xa3, 0x45, 0x03, 0x23, 0xb5, 0x62, 0xd7,
  0xc,  0xf5, 0x27, 0xd1, 0xf8, 0x91, 0x3c, 0xac, 0x44, 0x22, 0x92, 0xef };

const unsigned char key2[8]={0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10};
const char ciph2[] = {
  0xca, 0x90, 0xf5, 0x9d, 0xcb, 0xd4, 0xd2, 0x3c, 0x01, 0x88, 0x7f, 0x3e,
  0x31, 0x6e, 0x62, 0x9d, 0xd8, 0xe0, 0x57, 0xa3, 0x06, 0x3a, 0x42, 0x58,
  0x2a, 0x28, 0xfe, 0x72, 0x52, 0x2f, 0xdd, 0xe0, 0x19, 0x89, 0x09, 0x1c,
  0x2a, 0x8e, 0x8c, 0x94, 0xfc, 0xc7, 0x68, 0xe4, 0x88, 0xaa, 0xde, 0x0f };

#ifdef TEST_KERNEL
#define fc_keysched    _afs_QTKrFdpoFL
#define fc_ecb_encrypt _afs_sDLThwNLok
#define fc_cbc_encrypt _afs_fkyCWTvfRS
#define rxkad_DecryptPacket _afs_SRWEeqTXrS
#define rxkad_EncryptPacket _afs_bpwQbdoghO
#endif

int rx_SlowPutInt32() { abort(); }

int
main()
{
  int32 sched[ROUNDS];
  char ciph[100], clear[100], tmp[100];
  u_int32 data[2];
  u_int32 iv[2];
  struct rx_packet packet;

  if (sizeof(int32) != 4)
    fprintf(stderr, "error: sizeof(int32) != 4\n");
  if (sizeof(u_int32) != 4)
    fprintf(stderr, "error: sizeof(u_int32) != 4\n");

  /*
   * Use key1 and key2 as iv */
  fc_keysched(key1, sched);
  memcpy(iv, key2, sizeof(iv));
  fc_cbc_encrypt(the_quick, ciph, sizeof(the_quick), sched, iv, ENCRYPT);
  if (memcmp(ciph1, ciph, sizeof(ciph1)) != 0)
    fprintf(stderr, "encrypt FAILED\n");
  memcpy(iv, key2, sizeof(iv));
  fc_cbc_encrypt(ciph, clear, sizeof(the_quick), sched, iv, DECRYPT);
  if (strcmp(the_quick, clear) != 0)
    fprintf(stderr, "crypt decrypt FAILED\n");

  /*
   * Use key2 and key1 as iv
   */
  fc_keysched(key2, sched);
  memcpy(iv, key1, sizeof(iv));
  fc_cbc_encrypt(the_quick, ciph, sizeof(the_quick), sched, iv, ENCRYPT);
  if (memcmp(ciph2, ciph, sizeof(ciph2)) != 0)
    fprintf(stderr, "encrypt FAILED\n");
  memcpy(iv, key1, sizeof(iv));
  fc_cbc_encrypt(ciph, clear, sizeof(the_quick), sched, iv, DECRYPT);
  if (strcmp(the_quick, clear) != 0)
    fprintf(stderr, "crypt decrypt FAILED\n");

  /*
   * Test Encrypt- and Decrypt-Packet, use key1 and key2 as iv
   */
  fc_keysched(key1, sched);
  memcpy(iv, key2, sizeof(iv));
  strlcpy(clear, the_quick, sizeof(clear));
  packet.wirevec[1].iov_base = clear;
  packet.wirevec[1].iov_len = sizeof(the_quick);
  packet.wirevec[2].iov_len = 0;

  /* For unknown reasons bytes 4-7 are zeroed in rxkad_EncryptPacket */
  rxkad_EncryptPacket(tmp, sched, iv, sizeof(the_quick), &packet);
  rxkad_DecryptPacket(tmp, sched, iv, sizeof(the_quick), &packet);
  clear[4] ^= 'q';
  clear[5] ^= 'u';
  clear[6] ^= 'i';
  clear[7] ^= 'c';
  if (strcmp(the_quick, clear) != 0)
    fprintf(stderr, "rxkad_EncryptPacket/rxkad_DecryptPacket FAILED\n");

  {
    struct timeval start, stop;
    int i;
    
    fc_keysched(key1, sched);
    gettimeofday(&start, 0);
    for (i = 0; i < 1000000; i++)
      fc_keysched(key1, sched);
    gettimeofday(&stop, 0);
    printf("fc_keysched    = %2.2f us\n",
	   (stop.tv_sec - start.tv_sec
	    + (stop.tv_usec - start.tv_usec)/1e6)*1);
	   
    fc_ecb_encrypt(data, data, sched, ENCRYPT);
    gettimeofday(&start, 0);
    for (i = 0; i < 1000000; i++)
      fc_ecb_encrypt(data, data, sched, ENCRYPT);
    gettimeofday(&stop, 0);
    printf("fc_ecb_encrypt = %2.2f us\n",
	   (stop.tv_sec - start.tv_sec
	    + (stop.tv_usec - start.tv_usec)/1e6)*1);
	   
    fc_cbc_encrypt(the_quick, ciph, sizeof(the_quick), sched, iv, ENCRYPT);
    gettimeofday(&start, 0);
    for (i = 0; i < 100000; i++)
      fc_cbc_encrypt(the_quick, ciph, sizeof(the_quick), sched, iv, ENCRYPT);
    gettimeofday(&stop, 0);
    printf("fc_cbc_encrypt = %2.2f us\n",
	   (stop.tv_sec - start.tv_sec
	    + (stop.tv_usec - start.tv_usec)/1e6)*10);
	   
  }

  exit(0);
}
#endif /* TEST */
