/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* $Id: schedule.h,v 1.1 1998/11/14 23:37:28 deraadt Exp $ */
/*
 * schedule.h: 
 * schedule events like retransmission and clean ups.
 */
 
#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include "state.h"
#include "packets.h"

#undef EXTERN
 
#ifdef _SCHEDULE_C_
#define EXTERN
#else
#define EXTERN extern
#endif

#define REKEY        0
#define TIMEOUT      1
#define CLEANUP      2
#define MODULUS      3
#define UPDATE       4

#define MAX_RETRIES  3          /* Resend a packet max. as often */

#define CLEANUP_TIMEOUT   60
#define MODULUS_TIMEOUT   75
#define RESPONDER_TIMEOUT 300
#define RETRANS_TIMEOUT   10
#define REKEY_TIMEOUT     360

struct schedule {
     struct schedule *next;
     time_t tm;
     int offset;
     int event;
     u_int8_t *cookie;
     u_int16_t cookie_size;
};

EXTERN void schedule_process(int sock);
EXTERN int schedule_next(void);
EXTERN int schedule_offset(int type, u_int8_t *cookie);
EXTERN void schedule_insert(int type, int off, u_int8_t *cookie, 
			    u_int16_t cookie_size);
EXTERN void schedule_remove(int type, u_int8_t *cookie);
EXTERN void init_schedule(void);

#endif /* _SCHEDULE_H */
