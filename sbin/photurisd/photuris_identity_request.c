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
/*
 * photuris_identity_request:
 * create a IDENTITY_REQUEST packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: photuris_identity_request.c,v 1.1 1998/11/14 23:37:26 deraadt Exp $";
#endif

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "photuris.h"
#include "packets.h"
#include "state.h"
#include "identity.h"
#include "encrypt.h"
#ifdef DEBUG
#include "packet.h"
#endif

int
photuris_identity_request(struct stateob *st, u_char *buffer, int *size)
{
	struct identity_message *header;
	u_int16_t rsize, asize, tmp;
	u_int8_t *p, *verifyp;

	rsize = *size;
	if (rsize < IDENTITY_MESSAGE_MIN)
	     return -1;	/* buffer not large enough */

	asize = IDENTITY_MESSAGE_MIN;               /* Actual size */
	rsize -= asize;                             /* Remaining size */

	header = (struct identity_message *) buffer;
	header->type = IDENTITY_REQUEST;

	/* Copy the cookies */
      	bcopy(st->icookie, header->icookie, COOKIE_SIZE);
	bcopy(st->rcookie, header->rcookie, COOKIE_SIZE);

	header->lifetime[0] = (st->olifetime >> 16) & 0xFF;
	header->lifetime[1] = (st->olifetime >>  8) & 0xFF;
	header->lifetime[2] =  st->olifetime        & 0xFF;
	bcopy(st->oSPI, header->SPI, SPI_SIZE );

	/* Choose identity parameters (choice + value) */
	p = IDENTITY_MESSAGE_CHOICE(header);       /* To Identity choice */
	tmp = rsize;                               /* Remaining size */

	/* Choose and Copy choice */
	if (choose_identity(st, p, &tmp, st->uSPIoattrib, 
			    st->uSPIoattribsize) == -1 )   
	     return -1;

	p += tmp; asize += tmp; rsize -= tmp;

	verifyp = p;

        /* Leave space for verification data */ 
        tmp = get_identity_verification_size(st, IDENTITY_MESSAGE_CHOICE(header)); 
 
        if (rsize < tmp) 
	     return -1; /* buffer not large enough */ 

	/* Zero the buffer, so we can hash over it */
	bzero(verifyp, tmp);

        p += tmp; asize += tmp; rsize -= tmp;

	if (rsize < st->oSPIattribsize)
	     return -1; /* buffer not large enough */

	/* Copy attributes and padding */
	bcopy(st->oSPIattrib, p, st->oSPIattribsize);
	asize += st->oSPIattribsize;
	rsize -= st->oSPIattribsize;
	p += st->oSPIattribsize;

	tmp = rsize;
        if(packet_create_padding(st, asize - IDENTITY_MESSAGE_MIN, 
				 p, &tmp) == -1) 
	     return -1; 
 
        p += tmp; asize += tmp; rsize -= tmp; 

        /* Create verification data */ 
        create_identity_verification(st, verifyp, (u_int8_t *)header, asize); 

#ifdef DEBUG2
	printf("Identity-Request (before encryption):\n");
	packet_dump((u_int8_t *)header, asize, 0);
#endif

	/* Encrypt the packet after SPI if wished for */
	packet_encrypt(st, IDENTITY_MESSAGE_CHOICE(header),
                       asize - IDENTITY_MESSAGE_MIN);

	*size = asize;
	return 0;
}
