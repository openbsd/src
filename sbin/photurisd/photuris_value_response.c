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
 * photuris_value_response:
 * create a VALUE_RESPONSE packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: photuris_value_response.c,v 1.1 1998/11/14 23:37:27 deraadt Exp $";
#endif

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "photuris.h"
#include "packets.h"
#include "state.h"
#include "exchange.h"

int
photuris_value_response(struct stateob *st, u_char *buffer, int *size)
			 
{
	struct value_response *header;
	u_int16_t asize, rsize, tmp;

	rsize = *size;
        if (rsize < VALUE_RESPONSE_MIN + st->oSPIoattribsize) 
             return -1; /* buffer not large enough */ 
 
	header = (struct value_response *)buffer;

        asize = VALUE_RESPONSE_MIN + st->oSPIoattribsize; 
        rsize -= asize; 
 
        /* Generate an exchangevalue if not done already */ 
        tmp = rsize; 
        if(exchange_value_generate(st, VALUE_RESPONSE_VALUE(header), &tmp) == -1\
) 
             return -1; 
 
        asize += tmp; 
        bcopy(st->oSPIoattrib, VALUE_RESPONSE_VALUE(header)+tmp, 
	      st->oSPIoattribsize); 

	header = (struct value_response *) buffer;
	header->type = VALUE_RESPONSE;

      	bcopy(st->icookie, header->icookie, COOKIE_SIZE);
	bcopy(st->rcookie, header->rcookie, COOKIE_SIZE);

	bzero(header->reserved, sizeof(header->reserved)); /* zero for now */
	bzero(st->oSPITBV, 3);

	*size = asize;
	return 0;
}
