/*	$OpenBSD: tcfs_cipher_BLOWFISH.c,v 1.2 2000/06/17 17:32:26 provos Exp $	*/
/*
 * Copyright 2000 The TCFS Project at http://tcfs.dia.unisa.it/
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include "tcfs_cipher.h"
#include "crypto/blf.h"


void *
BLOWFISH_init_key (char *key)
{
	blf_ctx *ks=NULL;

	ks=(blf_ctx *)malloc (sizeof (blf_ctx), M_FREE, M_NOWAIT);
	if (!ks)
		return NULL;

	blf_key (ks, key, BLOWFISH_KEYSIZE);

	return (void *)ks;
}

void
BLOWFISH_cleanup_key(void *k)
{
	free((blf_ctx *)k, M_FREE);
}

void
BLOWFISH_encrypt(char *block, int nb, void *key)
{
	char iv[] = {'\0','\0','\0','\0','\0','\0','\0','\0'};
	blf_cbc_encrypt((blf_ctx *)key, iv, block, nb);
}

void
BLOWFISH_decrypt(char *block, int nb, void *key)
{
	char iv[] = {'\0','\0','\0','\0','\0','\0','\0','\0'};
	blf_cbc_decrypt((blf_ctx *)key, iv, block, nb);
}
