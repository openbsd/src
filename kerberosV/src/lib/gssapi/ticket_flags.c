/*
 * Copyright (c) 2004 Kungliga Tekniska Högskolan
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

#include "gssapi_locl.h"

RCSID("$KTH: ticket_flags.c,v 1.1 2004/03/19 14:55:50 lha Exp $");

OM_uint32
gss_krb5_get_tkt_flags(OM_uint32 *minor_status,
		       gss_ctx_id_t context_handle,
		       OM_uint32 *tkt_flags)
{
    if (context_handle == GSS_C_NO_CONTEXT) {
	*minor_status = EINVAL;
	return GSS_S_NO_CONTEXT;
    }
    HEIMDAL_MUTEX_lock(&context_handle->ctx_id_mutex);

    if (context_handle->ticket == NULL) {
	HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);
	*minor_status = EINVAL;
	return GSS_S_BAD_MECH;
    }

    *tkt_flags = TicketFlags2int(context_handle->ticket->ticket.flags);
    HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);

    *minor_status = 0;
    return GSS_S_COMPLETE;
}
