/*	$OpenBSD: skeytest.c,v 1.3 2008/06/26 05:42:05 ray Exp $	*/
/*	$NetBSD: skeytest.c,v 1.3 2002/02/21 07:38:18 itojun Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* This is a regression test for the S/Key implementation
	against the data set from Appendix C of RFC2289 */

#include <stdio.h>
#include <string.h>
#include "skey.h"

struct regRes {
	char *algo, *zero, *one, *nine;
	};

struct regPass {
	char *passphrase, *seed;
	struct regRes res[4];
	} regPass[] = {
		{ "This is a test.", "TeSt", { 
			{ "md4", "D1854218EBBB0B51", "63473EF01CD0B444", "C5E612776E6C237A" }, 
			{ "md5", "9E876134D90499DD", "7965E05436F5029F", "50FE1962C4965880" },
			{ "sha1","BB9E6AE1979D8FF4", "63D936639734385B", "87FEC7768B73CCF9" },
			{ NULL } } },
		{ "AbCdEfGhIjK", "alpha1", { 
			{ "md4", "50076F47EB1ADE4E", "65D20D1949B5F7AB", "D150C82CCE6F62D1" }, 
			{ "md5", "87066DD9644BF206", "7CD34C1040ADD14B", "5AA37A81F212146C" },
			{ "sha1","AD85F658EBE383C9", "D07CE229B5CF119B", "27BC71035AAF3DC6" },
			{ NULL } } },
		{ "OTP's are good", "correct", { 
			{ "md4", "849C79D4F6F55388", "8C0992FB250847B1", "3F3BF4B4145FD74B" },
			{ "md5", "F205753943DE4CF9", "DDCDAC956F234937", "B203E28FA525BE47" },
			{ "sha1","D51F3E99BF8E6F0B", "82AEB52D943774E4", "4F296A74FE1567EC" },
			{ NULL } } },
		{ NULL }
	};

int
main(int argc, char *argv[])
{
	char data[16], prn[64];
	struct regPass *rp;
	int i = 0;
	int errors = 0;
	int j;
	
	for(rp = regPass; rp->passphrase; rp++) {
		struct regRes *rr;

		i++;
		for(rr = rp->res; rr->algo; rr++) {
			skey_set_algorithm(rr->algo);

			keycrunch(data, rp->seed, rp->passphrase);
			btoa8(prn, data);

			if(strcasecmp(prn, rr->zero)) {
				errors++;
				printf("Set %d, round 0, %s: Expected %s and got %s\n",
				    i, rr->algo, rr->zero, prn);
			}

			f(data);
			btoa8(prn, data);

			if(strcasecmp(prn, rr->one)) {
				errors++;
				printf("Set %d, round 1, %s: Expected %s and got %s\n",
				    i, rr->algo, rr->one, prn);
			}

			for(j=1; j<99; j++)
				f(data);
			btoa8(prn, data);

			if(strcasecmp(prn, rr->nine)) {
				errors++;
				printf("Set %d, round 99, %s: Expected %s and got %s\n",
				    i, rr->algo, rr->nine, prn);
			}
		}
	}

	printf("%d errors\n", errors);
	return(errors ? 1 : 0);
}
