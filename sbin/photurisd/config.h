/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
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
/* $Id: config.h,v 1.1 1998/11/14 23:37:22 deraadt Exp $ */
/*
 * config.h: 
 * handling config
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "userdefs.h"

#ifdef MACHINE_ENDIAN
#include <machine/endian.h>
#endif

#ifdef ENDIAN
#include <endian.h>
#endif

#ifdef SYS_MACHINE
#include <sys/machine.h>
#endif

#ifdef SYS_LTYPES
#include <sys/ltypes.h>
#endif

#ifdef NEED_UTYPES
#include "utypes.h"
#endif

#ifdef NEED_IN_ADDR
# ifndef _IN_ADDR_T_
# define _IN_ADDR_T_
     typedef unsigned long in_addr_t;
# endif
#endif



#include "state.h"

#undef EXTERN
#ifdef _CONFIG_C_
#define EXTERN

#ifndef DEBUG
void reconfig(int sig);
#endif

#else
#define EXTERN extern
#endif

#define CONFIG_MODULUS      "modulus"
#define CONFIG_EXCHANGE     "exchange"
#define CONFIG_CONFIG       "config"
#define CONFIG_MAX_RETRIES  "exchange_max_retransmits"
#define CONFIG_RET_TIMEOUT  "exchange_retransmit_timeout"
#define CONFIG_EX_TIMEOUT   "exchange_timeout"
#define CONFIG_EX_LIFETIME  "exchange_lifetime"
#define CONFIG_SPI_LIFETIME "spi_lifetime"

#define OPT_DST             "dst"
#define OPT_PORT            "port"
#define OPT_OPTIONS         "options"
# define OPT_ENC            "enc"
# define OPT_AUTH           "auth"
#define OPT_USER            "user"
#define OPT_TSRC            "tsrc"
#define OPT_TDST            "tdst"

struct cfgx {
     struct cfgx *next;
     char *name;
     int id;
};

EXTERN int bin2hex(char *, int *, u_int8_t *, u_int16_t);
EXTERN char *chomp(char *);

EXTERN int init_moduli(int);
EXTERN int init_schemes(void);
EXTERN int init_attributes(void);
EXTERN int init_times(void);
EXTERN void startup_parse(struct stateob *st, char *line);
EXTERN void startup_end(struct stateob *st);
EXTERN int init_startup(void);
EXTERN int init_signals(void);

EXTERN int pick_scheme(u_int8_t **, u_int16_t *, u_int8_t *, u_int16_t);
EXTERN int pick_attrib(struct stateob *, u_int8_t **, u_int16_t *);
EXTERN int select_attrib(struct stateob *, u_int8_t **, u_int16_t *);

#endif /* _CONFIG_H_ */

