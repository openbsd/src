/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*
  Title:     Code to support byte-sex independence
  Copyright: (C) 1991, Advanced RISC Machines Ltd., Cambridge, England.
*/
/*
 * RCS $Revision: 1.3 $
 * Checkin $Date: 2004/12/27 14:00:53 $
 */

#ifndef angel_bytesex_h
#define angel_bytesex_h

#include "host.h"

void bytesex_reverse(int yes_or_no);
/*
 * Turn sex-reversal on or off - 0 means off, non-0 means on.
 */

int bytesex_reversing(void);
/*
 * Return non-0 if reversing the byte sex, else 0.
 */

int32 bytesex_hostval(int32 v);
/*
 * Return v or byte-reversed v, according to whether sex-reversval
 * is on or off.
 */

int32 bytesex_hostval_16(int32 v);
/* Return v or byte-reversed v for a 16 bit value */

#endif
