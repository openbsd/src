/*	$OpenBSD: tcfs_cipher_conf.c,v 1.2 2000/06/17 17:32:26 provos Exp $	*/
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
#include "tcfs_cipher.h"

struct tcfs_cipher tcfs_cipher_vect[]={
	{"3des",0,TDES_KEYSIZE,TDES_init_key,TDES_cleanup_key,
					TDES_encrypt,TDES_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
	{"bfish",0,BLOWFISH_KEYSIZE,BLOWFISH_init_key,BLOWFISH_cleanup_key,
					BLOWFISH_encrypt,BLOWFISH_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
	{"none",0,0,cnone_init_key,cnone_cleanup_key,
					cnone_encrypt,cnone_decrypt},
};

