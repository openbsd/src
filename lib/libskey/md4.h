/* 
 *
 * md4.h -- Header file for implementation of MD4 Message Digest Algorithm
 * Updated: 2/13/90 by Ronald L. Rivest
 * Reformatted and de-linted - 2/12/91 Phil Karn
 *
 * Copyright (C) 1990, RSA Data Security, Inc. All rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD4 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD4 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 *
 * $Id: md4.h,v 1.1.1.1 1995/10/18 08:43:11 deraadt Exp $
 */

#ifdef  __STDC__
#define __ARGS(X) X     /* For ANSI C */
#else
#define __ARGS(X) ()
#endif

/* MDstruct is the data structure for a message digest computation. */
typedef struct {
	unsigned long buffer[4];/* Holds 4-word result of MD computation */
	unsigned char count[8];	/* Number of bits processed so far */
	unsigned int done;	/* Nonzero means MD computation finished */
} MDstruct, *MDptr;

/* MDbegin(MD)
 * Input: MD -- an MDptr
 * Initialize the MDstruct prepatory to doing a message digest computation.
 */
extern void MDbegin __ARGS((MDptr MDp));

/* MDupdate(MD,X,count)
 * Input: MD -- an MDptr
 *        X -- a pointer to an array of unsigned characters.
 *        count -- the number of bits of X to use (an unsigned int).
 * Updates MD using the first ``count'' bits of X.
 * The array pointed to by X is not modified.
 * If count is not a multiple of 8, MDupdate uses high bits of last byte.
 * This is the basic input routine for a user.
 * The routine terminates the MD computation when count < 512, so
 * every MD computation should end with one call to MDupdate with a
 * count less than 512.  Zero is OK for a count.
 */
extern void MDupdate __ARGS((MDptr MDp,unsigned char *X,unsigned int count));

/* MDprint(MD)
 * Input: MD -- an MDptr
 * Prints message digest buffer MD as 32 hexadecimal digits.
 * Order is from low-order byte of buffer[0] to high-order byte of buffer[3].
 * Each byte is printed with high-order hexadecimal digit first.
 */
extern void MDprint __ARGS((MDptr MDp));

/* End of md4.h */
