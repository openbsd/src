/*
 * chap_ms.c - Microsoft MS-CHAP compatible implementation.
 *
 * Copyright (c) 1995 Eric Rosenquist, Strata Software Limited.
 * http://www.strataware.com/
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Eric Rosenquist.  The name of the author may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char rcsid[] = "$Id: chap_ms.c,v 1.1 1996/07/20 12:02:06 joshd Exp $";
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>

#include "pppd.h"
#include "chap.h"
#include "chap_ms.h"
#include "md4.h"


#ifdef CHAPMS
#include <des.h>

typedef struct {
    u_char LANManResp[24];
    u_char NTResp[24];
    u_char UseNT;		/* If 1, ignore the LANMan response field */
} MS_ChapResponse;
/* We use MS_CHAP_RESPONSE_LEN, rather than sizeof(MS_ChapResponse),
   in case this struct gets padded. */


static void	DesEncrypt __P((u_char *, u_char *, u_char *));
static void	MakeKey __P((u_char *, u_char *));


static void
ChallengeResponse(challenge, pwHash, response)
    u_char *challenge;	/* IN   8 octets */
    u_char *pwHash;	/* IN  16 octets */
    u_char *response;	/* OUT 24 octets */
{
    char    ZPasswordHash[21];

    BZERO(ZPasswordHash, sizeof(ZPasswordHash));
    BCOPY(pwHash, ZPasswordHash, 16);

#if 0
    log_packet(ZPasswordHash, sizeof(ZPasswordHash), "ChallengeResponse - ZPasswordHash");
#endif

    DesEncrypt(challenge, ZPasswordHash +  0, response + 0);
    DesEncrypt(challenge, ZPasswordHash +  7, response + 8);
    DesEncrypt(challenge, ZPasswordHash + 14, response + 16);

#if 0
    log_packet(response, 24, "ChallengeResponse - response");
#endif
}


static void
DesEncrypt(clear, key, cipher)
    u_char *clear;	/* IN  8 octets */
    u_char *key;	/* IN  7 octets */
    u_char *cipher;	/* OUT 8 octets */
{
    des_cblock		des_key;
    des_key_schedule	key_schedule;

    MakeKey(key, des_key);

    des_set_key(&des_key, key_schedule);

#if 0
    CHAPDEBUG((LOG_INFO, "DesEncrypt: 8 octet input : %02X%02X%02X%02X%02X%02X%02X%02X",
	       clear[0], clear[1], clear[2], clear[3], clear[4], clear[5], clear[6], clear[7]));
#endif

    des_ecb_encrypt((des_cblock *)clear, (des_cblock *)cipher, key_schedule, 1);

#if 0
    CHAPDEBUG((LOG_INFO, "DesEncrypt: 8 octet output: %02X%02X%02X%02X%02X%02X%02X%02X",
	       cipher[0], cipher[1], cipher[2], cipher[3], cipher[4], cipher[5], cipher[6], cipher[7]));
#endif
}


static u_char Get7Bits(input, startBit)
    u_char *input;
    int startBit;
{
    register unsigned int	word;

    word  = (unsigned)input[startBit / 8] << 8;
    word |= (unsigned)input[startBit / 8 + 1];

    word >>= 15 - (startBit % 8 + 7);

    return word & 0xFE;
}


static void MakeKey(key, des_key)
    u_char *key;	/* IN  56 bit DES key missing parity bits */
    u_char *des_key;	/* OUT 64 bit DES key with parity bits added */
{
    des_key[0] = Get7Bits(key,  0);
    des_key[1] = Get7Bits(key,  7);
    des_key[2] = Get7Bits(key, 14);
    des_key[3] = Get7Bits(key, 21);
    des_key[4] = Get7Bits(key, 28);
    des_key[5] = Get7Bits(key, 35);
    des_key[6] = Get7Bits(key, 42);
    des_key[7] = Get7Bits(key, 49);

    des_set_odd_parity((des_cblock *)des_key);

#if 0
    CHAPDEBUG((LOG_INFO, "MakeKey: 56-bit input : %02X%02X%02X%02X%02X%02X%02X",
	       key[0], key[1], key[2], key[3], key[4], key[5], key[6]));
    CHAPDEBUG((LOG_INFO, "MakeKey: 64-bit output: %02X%02X%02X%02X%02X%02X%02X%02X",
	       des_key[0], des_key[1], des_key[2], des_key[3], des_key[4], des_key[5], des_key[6], des_key[7]));
#endif
}

#endif /* CHAPMS */


void
ChapMS(cstate, rchallenge, rchallenge_len, secret, secret_len)
    chap_state *cstate;
    char *rchallenge;
    int rchallenge_len;
    char *secret;
    int secret_len;
{
#ifdef CHAPMS
    int			i;
    MDstruct		md4Context;
    MS_ChapResponse	response;
    u_char		unicodePassword[MAX_NT_PASSWORD * 2];

#if 0
    CHAPDEBUG((LOG_INFO, "ChapMS: secret is '%.*s'", secret_len, secret));
#endif

    BZERO(&response, sizeof(response));

    /* Initialize the Unicode version of the secret (== password). */
    /* This implicitly supports 8-bit ISO8859/1 characters. */
    BZERO(unicodePassword, sizeof(unicodePassword));
    for (i = 0; i < secret_len; i++)
	unicodePassword[i * 2] = (u_char)secret[i];

    MDbegin(&md4Context);
    MDupdate(&md4Context, unicodePassword, secret_len * 2 * 8);	/* Unicode is 2 bytes/char, *8 for bit count */
    MDupdate(&md4Context, NULL, 0);	/* Tell MD4 we're done */

    ChallengeResponse(rchallenge, (char *)md4Context.buffer, response.NTResp);

    response.UseNT = 1;

    BCOPY(&response, cstate->response, MS_CHAP_RESPONSE_LEN);
    cstate->resp_length = MS_CHAP_RESPONSE_LEN;
#endif /* CHAPMS */
}
