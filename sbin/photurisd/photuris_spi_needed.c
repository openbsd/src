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
/*
 * photuris_spi_needed:
 * 
 */

#ifndef lint
static char rcsid[] = "$Id: photuris_spi_needed.c,v 1.1 1998/11/14 23:37:27 deraadt Exp $";
#endif

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "packets.h"
#include "state.h"
#include "validity.h"
#include "encrypt.h"

int
photuris_spi_needed(struct stateob *st, u_char *buffer, int *size,
		    u_int8_t *attributes, u_int16_t attribsize)
{
        struct spi_needed *header;
        u_int16_t rsize, asize, tmp;
        u_int8_t *p;
 
        rsize = *size;
        if (rsize < SPI_NEEDED_MIN)
          return -1;    /* buffer not large enough */

	asize = SPI_NEEDED_MIN;                     /* Actual size */
        rsize -= asize;                             /* Remaining size */
 
        header = (struct spi_needed *) buffer;
        header->type = SPI_NEEDED;

	bzero(header->reserved, sizeof(header->reserved));
 
        /* Copy the cookies */
        bcopy(st->icookie, header->icookie, COOKIE_SIZE);
        bcopy(st->rcookie, header->rcookie, COOKIE_SIZE);

	p = SPI_NEEDED_VERIFICATION(header);

        /* Leave space for verification data */ 
        tmp = get_validity_verification_size(st); 
 
        if (rsize < tmp) 
          return -1; /* buffer not large enough */ 
 
        p += tmp; asize += tmp; rsize -= tmp;
 
        if (rsize < attribsize)
          return -1; /* buffer not large enough */
 
        /* Copy attributes and padding */
        bcopy(attributes, p, attribsize);
        asize += attribsize;
	rsize -= attribsize;
	p += attribsize;
 
	tmp = rsize;
        if(packet_create_padding(st, asize - SPI_NEEDED_MIN, p, &tmp) == -1)  
	     return -1;  
  
        p += tmp; asize += tmp; rsize -= tmp;  

        /* Create verification data */ 
        create_validity_verification(st,SPI_UPDATE_VERIFICATION(header),
				     (u_int8_t *)header,asize); 
 
        /* Encrypt the packet after header if wished for */
        packet_encrypt(st, SPI_NEEDED_VERIFICATION(header),
                       asize - SPI_NEEDED_MIN);
 
        *size = asize;
        return 0;
}
