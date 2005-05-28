/*
 * key.h
 *
 * Copyright (c) 2001 Dug Song <dugsong@arbor.net>
 * Copyright (c) 2001 Arbor Networks, Inc.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The names of the copyright holders may not be used to endorse or
 *      promote products derived from this software without specific
 *      prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *   THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Vendor: key.h,v 1.2 2005/04/01 16:47:31 dugsong Exp $
 */

#ifndef KEY_H
#define KEY_H

enum key_type {
	KEY_UNSPEC,
	KEY_RSA,
	KEY_DSA
};

struct key {
	int	 type;
	void	*data;
};

struct key	*key_new(void);
int		 key_load_public(struct key *k, char *filename);
int		 key_load_private(struct key *k, char *filename);
int		 key_sign(struct key *k, u_char *msg, int mlen,
	             u_char *sig, int slen);
int		 key_verify(struct key *k, u_char *msg, int mlen,
	             u_char *sig, int slen);
void		 key_free(struct key *k);

#endif /* KEY_H */
