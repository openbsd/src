/* --------------------------------- SHA1.H ------------------------------- */
   
/* NIST proposed Secure Hash Standard.
   
   Written 2 September 1992, Peter C. Gutmann.
   This implementation placed in the public domain.
   
   Comments to pgut1@cs.aukuni.ac.nz */

/* Useful defines/typedefs */

typedef unsigned char   BYTE;
typedef u_int32_t	LONG;

/* The SHA1 block size and message digest sizes, in bytes */

#define SHA1_BLOCKSIZE   64
#define SHA1_DIGESTSIZE  20

/* The structure for storing SHA1 info */

typedef struct {
	       LONG digest[ 5 ];            /* Message digest */
	       LONG countLo, countHi;       /* 64-bit bit count */
	       LONG data[ 16 ];             /* SHA1 data buffer */
	       } SHA1_INFO;

/* The next def turns on the change to the algorithm introduced by NIST at
 * the behest of the NSA.  It supposedly corrects a weakness in the original
 * formulation.  Bruce Schneier described it thus in a posting to the
 * Cypherpunks mailing list on June 21, 1994 (as told to us by Steve Bellovin):
 *
 *	This is the fix to the Secure Hash Standard, NIST FIPS PUB 180:
 *
 *	     In Section 7 of FIPS 180 (page 9), the line which reads
 *
 *	     "b) For t=16 to 79 let Wt = Wt-3 XOR Wt-8 XOR Wt-14 XOR
 *	     Wt-16."
 *
 *	     is to be replaced by
 *
 *	     "b) For t=16 to 79 let Wt = S1(Wt-3 XOR Wt-8 XOR Wt-14 XOR
 *	     Wt-16)."
 *
 *	     where S1 is a left circular shift by one bit as defined in
 *	     Section 3 of FIPS 180 (page 6):
 *
 *	     S1(X) = (X<<1) OR (X>>31).
 *
 */

#define NEW_SHA1
