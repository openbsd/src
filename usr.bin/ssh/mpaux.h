/*

mpaux.h

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Sun Jul 16 04:29:30 1995 ylo

This file contains various auxiliary functions related to multiple
precision integers.

*/

/* RCSID("$Id: mpaux.h,v 1.1 1999/09/26 20:53:36 deraadt Exp $"); */

#ifndef MPAUX_H
#define MPAUX_H

/* Converts a multiple-precision integer into bytes to be stored in the buffer.
   The buffer will contain the value of the integer, msb first. */
void mp_linearize_msb_first(unsigned char *buf, unsigned int len, 
			    MP_INT *value);

/* Extract a multiple-precision integer from buffer.  The value is stored
   in the buffer msb first. */
void mp_unlinearize_msb_first(MP_INT *value, const unsigned char *buf,
			      unsigned int len);

/* Computes a 16-byte session id in the global variable session_id.
   The session id is computed by concatenating the linearized, msb
   first representations of host_key_n, session_key_n, and the cookie. */
void compute_session_id(unsigned char session_id[16],
			unsigned char cookie[8],
			unsigned int host_key_bits,
			MP_INT *host_key_n,
			unsigned int session_key_bits,
			MP_INT *session_key_n);

#endif /* MPAUX_H */
