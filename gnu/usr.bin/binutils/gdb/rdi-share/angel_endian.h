/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/* -*-C-*-
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:53 $
 *
 *
 * angel_endian.h - target endianness independent read/write primitives.
 */

#ifndef angel_endian_h
#define angel_endian_h

/*
 * The endianness of the data being processed needs to be known, but
 * the host endianness is not required (since the data is constructed
 * using bytes).  At the moment these are provided as macros. This
 * gives the compiler freedom in optimising individual calls. However,
 * if space is at a premium then functions should be provided.
 *
 * NOTE: These macros assume that the data has been packed in the same format
 *       as the packing on the build host. If this is not the case then
 *       the wrong addresses could be used when dealing with structures.
 *
 */

/*
 * For all the following routines the target endianness is defined by the
 * following boolean definitions.
 */
#define BE (1 == 1) /* TRUE  : big-endian */
#define LE (1 == 0) /* FALSE : little-endian */

/*
 * The following type definitions are used by the endianness converting
 * macros.
 */
typedef unsigned char U8;
typedef U8 *P_U8;
typedef const U8 *CP_U8;

typedef unsigned short U16;
typedef U16 *P_U16;

typedef unsigned int U32;
typedef U32 *P_U32;

/*
 * If the endianness of the host and target are known (fixed) and the same
 * then the following macro definitions can be used. These just directly copy
 * the data.
 *
 * #define READ(e,a)       (a)
 * #define WRITE(e,a,v)    ((a) = (v))
 * #define PREAD(e,a)      (a)
 * #define PWRITE(e,a,v)   (*(a) = (v))
 */

/*
 * These macros assume that a byte (char) is 8bits in size, and that the
 * endianness is not important when reading or writing bytes.
 */
#define PUT8(a,v)       (*((P_U8)(a)) = (U8)(v))
#define PUT16LE(a,v)    (PUT8(a,((v) & 0xFF)), \
                         PUT8((((P_U8)(a)) + sizeof(char)),((v) >> 8)))
#define PUT16BE(a,v)    (PUT8(a,((v) >> 8)), \
                         PUT8((((P_U8)(a)) + sizeof(char)),((v) & 0xFF)))
#define PUT32LE(a,v)    (PUT16LE(a,v), \
                         PUT16LE((((P_U8)(a)) + sizeof(short)),((v) >> 16)))
#define PUT32BE(a,v)    (PUT16BE(a,((v) >> 16)), \
                         PUT16BE((((P_U8)(a)) + sizeof(short)),v))

#define GET8(a)     (*((CP_U8)(a)))
#define GET16LE(a)  (GET8(a) | (((U16)GET8(((CP_U8)(a)) + sizeof(char))) << 8))
#define GET16BE(a)  ((((U16)GET8(a)) << 8) | GET8(((CP_U8)(a)) + sizeof(char)))
#define GET32LE(a)  (GET16LE(a) | \
                     (((U32)GET16LE(((CP_U8)(a)) + sizeof(short))) << 16))
#define GET32BE(a)  ((((U32)GET16BE(a)) << 16) | \
                     GET16BE(((CP_U8)(a)) + sizeof(short)))

/*
 * These macros simplify the code in respect to reading and writing the
 * correct size data when dealing with endianness. "e" is TRUE if we are
 * dealing with big-endian data, FALSE if we are dealing with little-endian.
 */

/* void WRITE(int endianness, void *address, unsigned value); */

#define WRITE16(e,a,v) ((e) ? PUT16BE(&(a),v) : PUT16LE(&(a),v))
#define WRITE32(e,a,v) ((e) ? PUT32BE(&(a),v) : PUT32LE(&(a),v))
#define WRITE(e,a,v)   ((sizeof(v) == sizeof(char)) ? \
                        PUT8(&(a),v) : ((sizeof(v) == sizeof(short)) ? \
                                        WRITE16(e,a,v) : WRITE32(e,a,v)))

/* unsigned READ(int endianness, void *address) */
#define READ16(e,a) ((e) ? GET16BE(&(a)) : GET16LE(&(a)))
#define READ32(e,a) ((e) ? GET32BE(&(a)) : GET32LE(&(a)))
#define READ(e,a) ((sizeof(a) == sizeof(char)) ? \
                   GET8((CP_U8)&(a)) : ((sizeof(a) == sizeof(short)) ? \
                                       READ16(e,a) : READ32(e,a)))

/* void PWRITE(int endianness, void *address, unsigned value); */
#define PWRITE16(e,a,v) ((e) ? PUT16BE(a,v) : PUT16LE(a,v))
#define PWRITE32(e,a,v) ((e) ? PUT32BE(a,v) : PUT32LE(a,v))
#define PWRITE(e,a,v)   ((sizeof(v) == sizeof(char)) ? \
                         PUT8(a,v) : ((sizeof(v) == sizeof(short)) ? \
                                      PWRITE16(e,a,v) : PWRITE32(e,a,v)))

/* unsigned PREAD(int endianness, void *address) */
#define PREAD16(e,a) ((e) ? GET16BE(a) : GET16LE(a))
#define PREAD32(e,a) ((e) ? GET32BE(a) : GET32LE(a))
#define PREAD(e,a) ((sizeof(*(a)) == sizeof(char)) ? \
                    GET8((CP_U8)a) : ((sizeof(*(a)) == sizeof(short)) ? \
                                     PREAD16(e,a) : PREAD32(e,a)))

#endif /* !defined(angel_endian_h) */

/* EOF angel_endian.h */
