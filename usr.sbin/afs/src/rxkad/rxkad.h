/* -*- C -*- */

/*
 * Copyright (c) 1995 - 2001, 2003 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* @(#)$arla: rxkad.h,v 1.11 2003/06/10 16:15:39 lha Exp $ */

#ifndef __RXKAD_H
#define __RXKAD_H

#ifdef HAVE_STDS_H
#include <stds.h>
#endif

/* Krb4 tickets can't have a key version number of 256. This is used
 * as a magic kvno to indicate that this is really a krb5 ticket. The
 * real kvno can be retrieved from the cleartext portion of the
 * ticket. For more info see the Transarc header file rxkad.h.
 */
#define RXKAD_TKT_TYPE_KERBEROS_V5 256

/* Is this really large enough for a krb5 ticket? */
#define MAXKRB5TICKETLEN	1024
#define MAXKRB4TICKETLEN	1024

typedef char rxkad_level;
#define rxkad_clear 0		/* checksum some selected header fields */
#define rxkad_auth 1		/* rxkad_clear + protected packet length */
#define rxkad_crypt 2		/* rxkad_crypt + encrypt packet payload */
extern int rxkad_min_level;	/* enforce min level at client end */

extern int rxkad_EpochWasSet;

#ifndef __P
#define __P(x) x
#endif

struct rx_connection;

int32_t rxkad_GetServerInfo __P((struct rx_connection *con,
			       rxkad_level *level,
			       uint32_t *expiration,
			       char *name,
			       char *instance,
			       char *cell,
			       int32_t *kvno));

struct rx_securityClass *
rxkad_NewServerSecurityObject __P((/*rxkad_level*/ int min_level,
				   void *appl_data,
				   int (*get_key)(void *appl_data,
						  int kvno,
						  void *key),
				   int (*user_ok)(char *name,
						  char *inst,
						  char *realm,
						  int kvno)));

struct rx_securityClass *
rxkad_NewClientSecurityObject __P((/*rxkad_level*/ int level,
				   void *sessionkey,
				   int32_t kvno,
				   int ticketLen,
				   char *ticket));

#define RXKADINCONSISTENCY	(19270400L)
#define RXKADPACKETSHORT	(19270401L)
#define RXKADLEVELFAIL		(19270402L)
#define RXKADTICKETLEN		(19270403L)
#define RXKADOUTOFSEQUENCE	(19270404L)
#define RXKADNOAUTH		(19270405L)
#define RXKADBADKEY		(19270406L)
#define RXKADBADTICKET		(19270407L)
#define RXKADUNKNOWNKEY		(19270408L)
#define RXKADEXPIRED		(19270409L)
#define RXKADSEALEDINCON	(19270410L)
#define RXKADDATALEN		(19270411L)
#define RXKADILLEGALLEVEL	(19270412L)

/* The rest is backwards compatibility stuff that we don't use! */
#define MAXKTCTICKETLIFETIME (30*24*60*60)
#define MINKTCTICKETLEN (32)
#define MAXKTCTICKETLEN (344)
#define MAXKTCNAMELEN (64)
#define MAXKTCREALMLEN (64)

/*
#define MAXKTCNAMELEN		ANAME_SZ
#define MAXKTCREALMLEN		REALM_SZ
*/

#ifndef CLOCK_SKEW
#define CLOCK_SKEW (5*60)
#endif

#define KTC_TIME_UNCERTAINTY (CLOCK_SKEW)

/*
#define KTC_TIME_UNCERTAINTY	(60*15)
*/

struct ktc_encryptionKey {
  char data[8];
};

struct ktc_principal {
  char name[MAXKTCNAMELEN];
  char instance[MAXKTCNAMELEN];
  char cell[MAXKTCREALMLEN];
};

uint32_t life_to_time __P((uint32_t start, int life_));

int time_to_life __P((uint32_t start, uint32_t end));

int tkt_CheckTimes __P((int32_t begin, int32_t end, int32_t now));

int
tkt_MakeTicket __P((char *ticket,
		    int *ticketLen,
		    struct ktc_encryptionKey *key,
		    char *name, char *inst, char *cell,
		    uint32_t start, uint32_t end,
		    struct ktc_encryptionKey *sessionKey,
		    uint32_t host,
		    char *sname, char *sinst));

int
tkt_DecodeTicket __P((char *asecret,
		      int32_t ticketLen,
		      struct ktc_encryptionKey *key,
		      char *name,
		      char *inst,
		      char *cell,
		      char *sessionKey,
		      int32_t *host,
		      int32_t *start,
		      int32_t *end));

#endif /* __RXKAD_H */
