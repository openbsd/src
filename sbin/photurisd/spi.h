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
/* $Id: spi.h,v 1.2 1999/03/27 21:18:02 provos Exp $ */
/*
 * spi.h: 
 * security paramter index creation.
 */
 
#ifndef _SPI_H_
#define _SPI_H_

#include "state.h"

#undef EXTERN
 
#ifdef _SPI_C_
#define EXTERN
#else
#define EXTERN extern
#endif

#define SPI_LIFETIME   1800            /* 30 minutes default lifetime */

#define SPI_OWNER      1
#define SPI_NOTIFY     2
#define SPI_TUNNEL     4
#define SPI_ESP	       8	       /* Is used for ESP */

struct spiob {
     struct spiob *next;            /* Linked list */
     char *address;
     char *local_address;
     in_addr_t isrc, ismask;
     in_addr_t idst, idmask;
     int flags;
     u_int8_t SPI[SPI_SIZE];           /* SPI */ 
     u_int8_t icookie[COOKIE_SIZE];    /* Initator cookie */
     u_int8_t *attributes;             /* SPI attributes */
     u_int16_t attribsize;
     u_int8_t *sessionkey;             /* to be delete after use */
     u_int16_t sessionkeysize; 
     time_t lifetime;                  /* Lifetime for the SPI */
};

EXTERN time_t getspilifetime(struct stateob *st);
EXTERN int make_spi(struct stateob *st, char *local_address,
		    u_int8_t *SPI, time_t *lifetime, 
		    u_int8_t **attributes, u_int16_t *attribsize);

EXTERN int spi_set_tunnel(struct stateob *st, struct spiob *spi);
EXTERN int spi_insert(struct spiob *);
EXTERN int spi_unlink(struct spiob *);
EXTERN struct spiob *spi_new(char *, u_int8_t *);
EXTERN int spi_value_reset(struct spiob *);
EXTERN struct spiob *spi_find_attrib(char *address,
				     u_int8_t *attrib, u_int16_t attribsize);
EXTERN struct spiob *spi_find(char *, u_int8_t *);
EXTERN struct spiob *spi_root(void);
EXTERN void spi_cleanup(void);
EXTERN void spi_expire(void);

#endif /* _SPI_H */
