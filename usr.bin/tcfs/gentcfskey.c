/*	$OpenBSD: gentcfskey.c,v 1.2 2000/06/19 20:35:47 fgsch Exp $	*/

/*
 *	Transparent Cryptographic File System (TCFS) for NetBSD 
 *	Author and mantainer: 	Luigi Catuogno [luicat@tcfs.unisa.it]
 *	
 *	references:		http://tcfs.dia.unisa.it
 *				tcfs-bsd@tcfs.unisa.it
 */

/*
 *	Base utility set v0.1
 */

#include <stdio.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <err.h>
#include <md5.h>

#include <miscfs/tcfs/tcfs.h>
#include "tcfsdefines.h"

u_char *
gentcfskey (void)
{
	u_char *buff;
	MD5_CTX ctx, ctx2;
	u_int32_t tmp[KEYSIZE];
	u_char digest[16];
	int i;
	
	buff = (u_char *)calloc(KEYSIZE + 1, sizeof(char));

	/* Generate random key */
	for (i = 0; i < KEYSIZE; i ++)
		tmp[i] = arc4random();

	MD5Init(&ctx);
	for (i = 0; i < KEYSIZE; i += 16) {
		MD5Update(&ctx, (u_char *)tmp, sizeof(tmp));
		ctx2 = ctx;
		MD5Final(digest, &ctx2);
		memcpy(buff + i, digest, KEYSIZE - i > 16 ? 16 : KEYSIZE - i);
	}
	buff[KEYSIZE] = '\0';
	memset(&ctx, 0, sizeof(ctx));

	return (buff);
}
