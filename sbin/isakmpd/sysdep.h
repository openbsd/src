/* $OpenBSD: sysdep.h,v 1.15 2004/04/15 18:39:26 deraadt Exp $	 */
/* $EOM: sysdep.h,v 1.17 2000/12/04 04:46:35 angelos Exp $	 */

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _SYSDEP_H_
#define _SYSDEP_H_

#include <sys/types.h>

#include "sysdep-os.h"

struct proto;
struct sa;
struct sockaddr;

extern void     sysdep_app_handler(int);
extern int      sysdep_app_open(void);
extern int      sysdep_cleartext(int, int);
extern void     sysdep_connection_check(char *);
extern int      sysdep_ipsec_delete_spi(struct sa *, struct proto *, int);
extern int      sysdep_ipsec_enable_sa(struct sa *, struct sa *);
extern u_int8_t *
sysdep_ipsec_get_spi(size_t *, u_int8_t, struct sockaddr *,
		     struct sockaddr *, u_int32_t);
extern int
sysdep_ipsec_group_spis(struct sa *, struct proto *,
			struct proto *, int);
extern int
sysdep_ipsec_set_spi(struct sa *, struct proto *, int,
		     struct sa *);
extern char    *sysdep_progname(void);
extern u_int32_t sysdep_random(void);
extern u_int8_t sysdep_sa_len(struct sockaddr *);

#endif				/* _SYSDEP_H_ */
