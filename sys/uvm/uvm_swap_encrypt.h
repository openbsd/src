/*	$OpenBSD: uvm_swap_encrypt.h,v 1.10 2015/05/06 04:00:10 dlg Exp $	*/

/*
 * Copyright 1999 Niels Provos <provos@citi.umich.edu>
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

#ifndef _UVM_SWAP_ENCRYPT_H
#define _UVM_SWAP_ENCRYPT_H

#define SWPENC_ENABLE	0
#define SWPENC_CREATED	1
#define SWPENC_DELETED	2
#define SWPENC_MAXID	3

#define CTL_SWPENC_NAMES { \
	{ "enable", CTLTYPE_INT }, \
	{ "keyscreated", CTLTYPE_INT }, \
	{ "keysdeleted", CTLTYPE_INT }, \
}

#define SWAP_KEY_EXPIRE (120 /*60 * 60*/)	/* time after that keys expire */
#define SWAP_KEY_SIZE	4		/* 128-bit keys */

struct swap_key {
	u_int32_t key[SWAP_KEY_SIZE];	/* secret key for swap range */
	u_int16_t refcount;		/* pages that still need it */
};

int swap_encrypt_ctl(int *, u_int, void *, size_t *, void *, size_t,
			  struct proc *);

void swap_encrypt(struct swap_key *,caddr_t, caddr_t, u_int64_t, size_t);
void swap_decrypt(struct swap_key *,caddr_t, caddr_t, u_int64_t, size_t);

void swap_key_cleanup(struct swap_key *);
void swap_key_prepare(struct swap_key *, int);

#define SWAP_KEY_GET(s,x)	do {					\
					if ((x)->refcount == 0) {	\
						swap_key_create(x);	\
					}				\
					(x)->refcount++;		\
				} while(0);

#define SWAP_KEY_PUT(s,x)	do {					\
					(x)->refcount--;		\
					if ((x)->refcount == 0) {	\
						swap_key_delete(x);	\
					}				\
				} while(0);

void swap_key_create(struct swap_key *);
void swap_key_delete(struct swap_key *);

extern int uvm_doswapencrypt;		/* swapencrypt enabled/disabled */
extern int uvm_swprekeyprint;
extern u_int uvm_swpkeyexpire;		/* expiry time for keys (tR) */
extern int swap_encrypt_initialized;

#endif /* _UVM_SWAP_ENCRYPT_H */
