/*
 * Copyright (c) 2000 - 2001, 2003 Kungliga Tekniska Högskolan
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

RCSID("$KTH: copy_ccache.c,v 1.7 2003/09/01 15:11:09 lha Exp $");

OM_uint32
gss_krb5_copy_ccache(OM_uint32 *minor_status,
		     gss_cred_id_t cred,
		     krb5_ccache out)
{
    krb5_error_code kret;

    HEIMDAL_MUTEX_lock(&cred->cred_id_mutex);

    if (cred->ccache == NULL) {
	HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
	*minor_status = EINVAL;
	return GSS_S_FAILURE;
    }

    kret = krb5_cc_copy_cache(gssapi_krb5_context, cred->ccache, out);
    HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
    if (kret) {
	*minor_status = kret;
	gssapi_krb5_set_error_string ();
	return GSS_S_FAILURE;
    }
    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32
gsskrb5_extract_authz_data_from_sec_context(OM_uint32 *minor_status,
					    gss_ctx_id_t context_handle,
					    int ad_type,
					    gss_buffer_t ad_data)
{
    krb5_error_code ret;
    krb5_data data;
    
    ad_data->value = NULL;
    ad_data->length = 0;
    
    HEIMDAL_MUTEX_lock(&context_handle->ctx_id_mutex);
    if (context_handle->ticket == NULL) {
	HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);
	*minor_status = EINVAL;
	return GSS_S_FAILURE;
    }

    ret = krb5_ticket_get_authorization_data_type(gssapi_krb5_context,
						  context_handle->ticket,
						  ad_type,
						  &data);
    HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    
    ad_data->value = malloc(data.length);
    if (ad_data->value == NULL) {
	krb5_data_free(&data);
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    ad_data->length = data.length;
    memcpy(ad_data->value, data.data, ad_data->length);
    krb5_data_free(&data);
	    
    *minor_status = 0;
    return GSS_S_COMPLETE;
}
