/*
 * Copyright 1997,1998 Niels Provos <provos@physnet.uni-hamburg.de>
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
/* $Id: state.h,v 1.1 1998/11/14 23:37:29 deraadt Exp $ */
/*
 * state.h: 
 * state object
 */

#ifndef _STATE_H_
#define _STATE_H_

#include <netinet/in.h>
#include <gmp.h>
#include <time.h>
#include "userdefs.h"
#ifdef NEED_UTYPES
#include "utypes.h"
#endif

#include "packets.h"

/* Possible values of flags */
#define IPSEC_OPT_ENC		0x0001  /* Negotiate encryption */
#define IPSEC_OPT_AUTH		0x0002  /* Negotiate authentication */
#define IPSEC_OPT_TUNNEL	0x0004  /* Negotiate tunne mode */
#define IPSEC_OPT_REPLAY	0x0100  /* Encryption with replay protection */
#define IPSEC_OPT_ENC_AUTH	0x0200  /* Encryption with authentication */
#define IPSEC_OPT_XOR		0x0400  /* Encryption with XOR */
#define IPSEC_OPT_COMPRESS	0x0800  /* Encryption with COMPRESS */
#define IPSEC_NOTIFY		0x1000  /* State created by kernel notify */

struct stateob {
  struct stateob *next;            /* Linked list */

  int initiator;                   /* Boolean */
  int phase;                       /* Actual phase in the exchange */

  char *user;                      /* User name for which do the exchange */
  int flags;                       /* Possible flags for this exchange */
  in_addr_t isrc, ismask;          /* Accept source for tunnel */
  in_addr_t idst, idmask;          /* Accept destination for tunnel */
  
  char address[16];                /* Remote address */
  u_int16_t port;                  /* Remote port for Photuris daemon */
  u_int16_t sport, dport;          /* Only used by notify at the moment */
  u_int8_t protocol;               /* to pass back to the kernel */

  u_int8_t icookie[COOKIE_SIZE];   /* Initator cookie */
  u_int8_t rcookie[COOKIE_SIZE];   /* Responder cookie */
  u_int8_t counter;                /* Connection counter */
  u_int8_t resource;               /* Received a resource limit */

  u_int8_t *verification;          /* Verification field of last touched message */
  u_int16_t versize;

  u_int8_t *scheme;                 /* Selected exchange scheme, holds gen. */
  u_int16_t schemesize;             /* Size including value ... */

  u_int8_t *roschemes;              /* Responder offered schemes */
  u_int16_t roschemesize;           /* Responder offered schemes size */

  u_int8_t oSPI[SPI_SIZE];          /* Owner SPI */ 
  u_int8_t oSPITBV[3];              /* Three Byte Value */
  u_int8_t *oSPIident;              /* Owner SPI identification */
  u_int8_t *oSPIattrib;             /* Owner SPI attributes */
  u_int16_t oSPIattribsize;
  u_int8_t *oSPIoattrib;            /* Owner SPI offered attributes */
  u_int16_t oSPIoattribsize;
  u_int8_t *oSPIsecret;             /* Owner SPI secret keys */
  u_int16_t oSPIsecretsize;
  u_int8_t *oSPIidentver;           /* Owner SPI Identity Verification */
  u_int16_t oSPIidentversize;
  u_int8_t *oSPIidentchoice;        /* Owner SPI Identity Choice */
  u_int16_t oSPIidentchoicesize;
  void *oSPIprivacyctx;
  time_t olifetime;                 /* Owner SPI lifetime */

  u_int8_t uSPI[SPI_SIZE];          /* User SPI */
  u_int8_t uSPITBV[3];              /* Three Byte Value */
  u_int8_t *uSPIident;              /* User SPI identification */
  u_int8_t *uSPIattrib;             /* User SPI attributes */
  u_int16_t uSPIattribsize;
  u_int8_t *uSPIoattrib;            /* User SPI offered attributes */
  u_int16_t uSPIoattribsize;
  u_int8_t *uSPIsecret;             /* User SPI secret keys */
  u_int16_t uSPIsecretsize;
  u_int8_t *uSPIidentver;           /* User SPI Identity Verification */
  u_int16_t uSPIidentversize;
  u_int8_t *uSPIidentchoice;        /* User SPI Identity Choice */
  u_int16_t uSPIidentchoicesize;
  void *uSPIprivacyctx;
  time_t ulifetime;                 /* User SPI lifetime */

  mpz_t modulus;                    /* Modulus for look up in cache */
  mpz_t generator;                  /* Generator for look up in cache */
  u_int8_t *texchange;              /* Their exchange value */
  u_int16_t texchangesize;
  u_int8_t *exchangevalue;          /* Our exchange value */
  u_int16_t exchangesize;
  u_int8_t *shared;                 /* Shared secret */
  u_int16_t sharedsize;

  int retries;                      /* Number of retransmits */
  u_int8_t *packet;                 /* Buffer for retransmits */
  u_int16_t packetlen;
  u_int8_t packetsig[16];           /* MD5 hash of an old packet */

  time_t lifetime;                  /* Lifetime for the exchange */
  time_t exchange_lifetime;         /* Use this as default */
  time_t spi_lifetime;              /* Use this as default */
};

/* Prototypes */
int state_insert(struct stateob *);
int state_unlink(struct stateob *);
struct stateob *state_new(void);
int state_value_reset(struct stateob *);
struct stateob *state_root(void);
struct stateob *state_find(char *);
struct stateob *state_find_next(struct stateob *, char *);
struct stateob *state_find_cookies(char *, u_int8_t *, u_int8_t *);
int state_save_verification(struct stateob *st, u_int8_t *buf, u_int16_t len);
void state_copy_flags(struct stateob *src, struct stateob *dst);
void state_cleanup(void);
void state_expire(void);

#define EXCHANGE_TIMEOUT       60
#define EXCHANGE_LIFETIME    1800

#endif
