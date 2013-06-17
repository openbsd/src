#include <tommath.h>
#ifdef BN_ERROR_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtom.org
 */

static const struct {
     int code;
     char *msg;
} msgs[] = {
     { MP_OKAY, "Successful" },
     { MP_MEM,  "Out of heap" },
     { MP_VAL,  "Value out of range" }
};

/* return a char * string for a given code */
char *mp_error_to_string(int code)
{
   int x;

   /* scan the lookup table for the given message */
   for (x = 0; x < (int)(sizeof(msgs) / sizeof(msgs[0])); x++) {
       if (msgs[x].code == code) {
          return msgs[x].msg;
       }
   }

   /* generic reply for invalid code */
   return "Invalid error code";
}

#endif

/* $Source: /home/cvs/src/kerberosV/src/lib/hcrypto/libtommath/Attic/bn_error.c,v $ */
/* $Revision: 1.1 $ */
/* $Date: 2013/06/17 19:11:43 $ */
