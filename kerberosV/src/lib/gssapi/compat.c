/*
 * Copyright (c) 2003 - 2005 Kungliga Tekniska Högskolan
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

RCSID("$KTH: compat.c,v 1.10 2005/05/30 20:51:51 lha Exp $");


krb5_error_code
_gss_check_compat(OM_uint32 *minor_status, gss_name_t name, 
		  const char *option, krb5_boolean *compat, 
		  krb5_boolean match_val)
{
    krb5_error_code ret = 0;
    char **p, **q;
    krb5_principal match;


    p = krb5_config_get_strings(gssapi_krb5_context, NULL, "gssapi",
				option, NULL);
    if(p == NULL)
	return 0;

    match = NULL;
    for(q = p; *q; q++) {
	ret = krb5_parse_name(gssapi_krb5_context, *q, &match);
	if (ret)
	    break;

	if (krb5_principal_match(gssapi_krb5_context, name, match)) {
	    *compat = match_val;
	    break;
	}
	
	krb5_free_principal(gssapi_krb5_context, match);
	match = NULL;
    }
    if (match)
	krb5_free_principal(gssapi_krb5_context, match);
    krb5_config_free_strings(p);

    if (ret) {
	if (minor_status)
	    *minor_status = ret;
	return GSS_S_FAILURE;
    }

    return 0;
}

/*
 * ctx->ctx_id_mutex is assumed to be locked
 */

OM_uint32
_gss_DES3_get_mic_compat(OM_uint32 *minor_status, gss_ctx_id_t ctx)
{
    krb5_boolean use_compat = FALSE;
    OM_uint32 ret;

    if ((ctx->more_flags & COMPAT_OLD_DES3_SELECTED) == 0) {
	ret = _gss_check_compat(minor_status, ctx->target, 
				"broken_des3_mic", &use_compat, TRUE);
	if (ret)
	    return ret;
	ret = _gss_check_compat(minor_status, ctx->target, 
				"correct_des3_mic", &use_compat, FALSE);
	if (ret)
	    return ret;

	if (use_compat)
	    ctx->more_flags |= COMPAT_OLD_DES3;
	ctx->more_flags |= COMPAT_OLD_DES3_SELECTED;
    }
    return 0;
}

OM_uint32
gss_krb5_compat_des3_mic(OM_uint32 *minor_status, gss_ctx_id_t ctx, int on)
{
    *minor_status = 0;

    HEIMDAL_MUTEX_lock(&ctx->ctx_id_mutex);
    if (on) {
	ctx->more_flags |= COMPAT_OLD_DES3;
    } else {
	ctx->more_flags &= ~COMPAT_OLD_DES3;
    }
    ctx->more_flags |= COMPAT_OLD_DES3_SELECTED;
    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);

    return 0;
}

/*
 * For compatability with the Windows SPNEGO implementation, the
 * default is to ignore the mechListMIC unless the initiator specified
 * CFX or configured in krb5.conf with the option
 * 	[gssapi]require_mechlist_mic=target-principal-pattern.
 * The option is valid for both initiator and acceptor.
 */
OM_uint32
_gss_spnego_require_mechlist_mic(OM_uint32 *minor_status,
				 gss_ctx_id_t ctx,
				 krb5_boolean *require_mic)
{
    OM_uint32 ret;
    int is_cfx = 0;

    gsskrb5_is_cfx(ctx, &is_cfx);
    if (is_cfx) {
	/* CFX session key was used */
	*require_mic = TRUE;
    } else {
	*require_mic = FALSE;
	ret = _gss_check_compat(minor_status, ctx->target, 
				"require_mechlist_mic",
				require_mic, TRUE);
	if (ret)
	    return ret;
    }
    *minor_status = 0;
    return GSS_S_COMPLETE;
}
