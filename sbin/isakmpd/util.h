/* $OpenBSD: util.h,v 1.19 2004/05/23 16:14:22 deraadt Exp $	 */
/* $EOM: util.h,v 1.10 2000/10/24 13:33:39 niklas Exp $	 */

/*
 * Copyright (c) 1998 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2001 Håkan Olsson.  All rights reserved.
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

#ifndef _UTIL_H_
#define _UTIL_H_

#include <sys/types.h>

extern int      allow_name_lookups;
extern int      regrand;
extern unsigned long seed;

struct message;
struct sockaddr;

extern int      check_file_secrecy(char *, size_t *);
extern int      check_file_secrecy_fd(int, char *, size_t *);
extern u_int16_t decode_16(u_int8_t *);
extern u_int32_t decode_32(u_int8_t *);
extern u_int64_t decode_64(u_int8_t *);
#if 0
extern void     decode_128(u_int8_t *, u_int8_t *);
#endif
extern void     encode_16(u_int8_t *, u_int16_t);
extern void     encode_32(u_int8_t *, u_int32_t);
extern void     encode_64(u_int8_t *, u_int64_t);
#if 0
extern void     encode_128(u_int8_t *, u_int8_t *);
#endif
extern u_int8_t *getrandom(u_int8_t *, size_t);
extern int      hex2raw(char *, u_int8_t *, size_t);
extern int      ones_test(const u_int8_t *, size_t);
extern int      sockaddr2text(struct sockaddr *, char **, int);
extern u_int8_t *sockaddr_addrdata(struct sockaddr *);
extern int      sockaddr_addrlen(struct sockaddr *);
extern in_port_t sockaddr_port(struct sockaddr *);
extern int      text2sockaddr(char *, char *, struct sockaddr **);
extern void     util_ntoa(char **, int, u_int8_t *);
extern int      zero_test(const u_int8_t *, size_t);

#endif				/* _UTIL_H_ */
