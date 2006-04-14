/*
 * Copyright (c) 1997, 2003 Kungliga Tekniska Högskolan
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

RCSID("$KTH: inquire_cred.c,v 1.7 2004/11/30 19:27:11 lha Exp $");

OM_uint32 gss_inquire_cred
           (OM_uint32 * minor_status,
            const gss_cred_id_t cred_handle,
            gss_name_t * name,
            OM_uint32 * lifetime,
            gss_cred_usage_t * cred_usage,
            gss_OID_set * mechanisms
           )
{
    gss_cred_id_t cred;
    OM_uint32 ret;

    *minor_status = 0;

    if (name)
	*name = NULL;
    if (mechanisms)
	*mechanisms = GSS_C_NO_OID_SET;

    if (cred_handle == GSS_C_NO_CREDENTIAL) {
	ret = gss_acquire_cred(minor_status, 
			       GSS_C_NO_NAME,
			       GSS_C_INDEFINITE,
			       GSS_C_NO_OID_SET,
			       GSS_C_BOTH,
			       &cred,
			       NULL,
			       NULL);
	if (ret)
	    return ret;
    } else
	cred = (gss_cred_id_t)cred_handle;

    HEIMDAL_MUTEX_lock(&cred->cred_id_mutex);

    if (name != NULL) {
	if (cred->principal != NULL) {
            ret = gss_duplicate_name(minor_status, cred->principal,
		name);
            if (ret)
		goto out;
	} else if (cred->usage == GSS_C_ACCEPT) {
	    *minor_status = krb5_sname_to_principal(gssapi_krb5_context, NULL,
		NULL, KRB5_NT_SRV_HST, name);
	    if (*minor_status) {
		ret = GSS_S_FAILURE;
		goto out;
	    }
	} else {
	    *minor_status = krb5_get_default_principal(gssapi_krb5_context,
		name);
	    if (*minor_status) {
		ret = GSS_S_FAILURE;
		goto out;
	    }
	}
    }
    if (lifetime != NULL) {
	ret = gssapi_lifetime_left(minor_status, 
				   cred->lifetime,
				   lifetime);
	if (ret)
	    goto out;
    }
    if (cred_usage != NULL)
        *cred_usage = cred->usage;

    if (mechanisms != NULL) {
        ret = gss_create_empty_oid_set(minor_status, mechanisms);
        if (ret)
	    goto out;
        ret = gss_add_oid_set_member(minor_status,
				     &cred->mechanisms->elements[0],
				     mechanisms);
        if (ret)
	    goto out;
    }
    ret = GSS_S_COMPLETE;
 out:
    HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);

    if (cred_handle == GSS_C_NO_CREDENTIAL)
	ret = gss_release_cred(minor_status, &cred);

    return ret;
}
