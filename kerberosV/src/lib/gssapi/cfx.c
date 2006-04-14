/*
 * Copyright (c) 2003, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "gssapi_locl.h"

RCSID("$KTH: cfx.c,v 1.17 2005/04/27 17:47:32 lha Exp $");

/*
 * Implementation of draft-ietf-krb-wg-gssapi-cfx-06.txt
 */

#define CFXSentByAcceptor	(1 << 0)
#define CFXSealed		(1 << 1)
#define CFXAcceptorSubkey	(1 << 2)

static krb5_error_code
wrap_length_cfx(krb5_crypto crypto,
		int conf_req_flag,
		size_t input_length,
		size_t *output_length,
		size_t *cksumsize,
		u_int16_t *padlength)
{
    krb5_error_code ret;
    krb5_cksumtype type;

    /* 16-byte header is always first */
    *output_length = sizeof(gss_cfx_wrap_token_desc);
    *padlength = 0;

    ret = krb5_crypto_get_checksum_type(gssapi_krb5_context, crypto, &type);
    if (ret) {
	return ret;
    }

    ret = krb5_checksumsize(gssapi_krb5_context, type, cksumsize);
    if (ret) {
	return ret;
    }

    if (conf_req_flag) {
	size_t padsize;

	/* Header is concatenated with data before encryption */
	input_length += sizeof(gss_cfx_wrap_token_desc);

	ret = krb5_crypto_getpadsize(gssapi_krb5_context, crypto, &padsize);
	if (ret) {
	    return ret;
	}
	if (padsize > 1) {
	    /* XXX check this */
	    *padlength = padsize - (input_length % padsize);
	}

	/* We add the pad ourselves (noted here for completeness only) */
	input_length += *padlength;

	*output_length += krb5_get_wrapped_length(gssapi_krb5_context,
						  crypto, input_length);
    } else {
	/* Checksum is concatenated with data */
	*output_length += input_length + *cksumsize;
    }

    assert(*output_length > input_length);

    return 0;
}

OM_uint32 _gssapi_wrap_size_cfx(OM_uint32 *minor_status,
				const gss_ctx_id_t context_handle,
				int conf_req_flag,
				gss_qop_t qop_req,
				OM_uint32 req_output_size,
				OM_uint32 *max_input_size,
				krb5_keyblock *key)
{
    krb5_error_code ret;
    krb5_crypto crypto;
    u_int16_t padlength;
    size_t output_length, cksumsize;

    ret = krb5_crypto_init(gssapi_krb5_context, key, 0, &crypto);
    if (ret != 0) {
	gssapi_krb5_set_error_string();
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = wrap_length_cfx(crypto, conf_req_flag, 
			  req_output_size,
			  &output_length, &cksumsize, &padlength);
    if (ret != 0) {
	gssapi_krb5_set_error_string();
	*minor_status = ret;
	krb5_crypto_destroy(gssapi_krb5_context, crypto);
	return GSS_S_FAILURE;
    }

    if (output_length < req_output_size) {
	*max_input_size = (req_output_size - output_length);
	*max_input_size -= padlength;
    } else {
	/* Should this return an error? */
	*max_input_size = 0;
    }

    krb5_crypto_destroy(gssapi_krb5_context, crypto);

    return GSS_S_COMPLETE;
}

/*
 * Rotate "rrc" bytes to the front or back
 */

static krb5_error_code
rrc_rotate(void *data, size_t len, u_int16_t rrc, krb5_boolean unrotate)
{
    u_char *tmp;
    size_t left;
    char buf[256];

    if (len == 0)
	return 0;

    rrc %= len;

    if (rrc == 0)
	return 0;

    left = len - rrc;

    if (rrc <= sizeof(buf)) {
	tmp = buf;
    } else {
	tmp = malloc(rrc);
	if (tmp == NULL) 
	    return ENOMEM;
    }
 
    if (unrotate) {
	memcpy(tmp, data, rrc);
	memmove(data, (u_char *)data + rrc, left);
	memcpy((u_char *)data + left, tmp, rrc);
    } else {
	memcpy(tmp, (u_char *)data + left, rrc);
	memmove((u_char *)data + rrc, data, left);
	memcpy(data, tmp, rrc);
    }

    if (rrc > sizeof(buf)) 
	free(tmp);

    return 0;
}

OM_uint32 _gssapi_wrap_cfx(OM_uint32 *minor_status,
			   const gss_ctx_id_t context_handle,
			   int conf_req_flag,
			   gss_qop_t qop_req,
			   const gss_buffer_t input_message_buffer,
			   int *conf_state,
			   gss_buffer_t output_message_buffer,
			   krb5_keyblock *key)
{
    krb5_crypto crypto;
    gss_cfx_wrap_token token;
    krb5_error_code ret;
    unsigned usage;
    krb5_data cipher;
    size_t wrapped_len, cksumsize;
    u_int16_t padlength, rrc = 0;
    OM_uint32 seq_number;
    u_char *p;

    ret = krb5_crypto_init(gssapi_krb5_context, key, 0, &crypto);
    if (ret != 0) {
	gssapi_krb5_set_error_string();
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = wrap_length_cfx(crypto, conf_req_flag, 
			  input_message_buffer->length,
			  &wrapped_len, &cksumsize, &padlength);
    if (ret != 0) {
	gssapi_krb5_set_error_string();
	*minor_status = ret;
	krb5_crypto_destroy(gssapi_krb5_context, crypto);
	return GSS_S_FAILURE;
    }

    /* Always rotate encrypted token (if any) and checksum to header */
    rrc = (conf_req_flag ? sizeof(*token) : 0) + (u_int16_t)cksumsize;

    output_message_buffer->length = wrapped_len;
    output_message_buffer->value = malloc(output_message_buffer->length);
    if (output_message_buffer->value == NULL) {
	*minor_status = ENOMEM;
	krb5_crypto_destroy(gssapi_krb5_context, crypto);
	return GSS_S_FAILURE;
    }

    p = output_message_buffer->value;
    token = (gss_cfx_wrap_token)p;
    token->TOK_ID[0] = 0x05;
    token->TOK_ID[1] = 0x04;
    token->Flags     = 0;
    token->Filler    = 0xFF;
    if ((context_handle->more_flags & LOCAL) == 0)
	token->Flags |= CFXSentByAcceptor;
    if (context_handle->more_flags & ACCEPTOR_SUBKEY)
	token->Flags |= CFXAcceptorSubkey;
    if (conf_req_flag) {
	/*
	 * In Wrap tokens with confidentiality, the EC field is
	 * used to encode the size (in bytes) of the random filler.
	 */
	token->Flags |= CFXSealed;
	token->EC[0] = (padlength >> 8) & 0xFF;
	token->EC[1] = (padlength >> 0) & 0xFF;
    } else {
	/*
	 * In Wrap tokens without confidentiality, the EC field is
	 * used to encode the size (in bytes) of the trailing
	 * checksum.
	 *
	 * This is not used in the checksum calcuation itself,
	 * because the checksum length could potentially vary
	 * depending on the data length.
	 */
	token->EC[0] = 0;
	token->EC[1] = 0;
    }

    /*
     * In Wrap tokens that provide for confidentiality, the RRC
     * field in the header contains the hex value 00 00 before
     * encryption.
     *
     * In Wrap tokens that do not provide for confidentiality,
     * both the EC and RRC fields in the appended checksum
     * contain the hex value 00 00 for the purpose of calculating
     * the checksum.
     */
    token->RRC[0] = 0;
    token->RRC[1] = 0;

    HEIMDAL_MUTEX_lock(&context_handle->ctx_id_mutex);
    krb5_auth_con_getlocalseqnumber(gssapi_krb5_context,
				    context_handle->auth_context,
				    &seq_number);
    gssapi_encode_be_om_uint32(0,          &token->SND_SEQ[0]);
    gssapi_encode_be_om_uint32(seq_number, &token->SND_SEQ[4]);
    krb5_auth_con_setlocalseqnumber(gssapi_krb5_context,
				    context_handle->auth_context,
				    ++seq_number);
    HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);

    /*
     * If confidentiality is requested, the token header is
     * appended to the plaintext before encryption; the resulting
     * token is {"header" | encrypt(plaintext | pad | "header")}.
     *
     * If no confidentiality is requested, the checksum is
     * calculated over the plaintext concatenated with the
     * token header.
     */
    if (context_handle->more_flags & LOCAL) {
	usage = KRB5_KU_USAGE_INITIATOR_SEAL;
    } else {
	usage = KRB5_KU_USAGE_ACCEPTOR_SEAL;
    }

    if (conf_req_flag) {
	/*
	 * Any necessary padding is added here to ensure that the
	 * encrypted token header is always at the end of the
	 * ciphertext.
	 *
	 * The specification does not require that the padding
	 * bytes are initialized.
	 */
	p += sizeof(*token);
	memcpy(p, input_message_buffer->value, input_message_buffer->length);
	memset(p + input_message_buffer->length, 0xFF, padlength);
	memcpy(p + input_message_buffer->length + padlength,
	       token, sizeof(*token));

	ret = krb5_encrypt(gssapi_krb5_context, crypto,
			   usage, p,
			   input_message_buffer->length + padlength +
				sizeof(*token),
			   &cipher);
	if (ret != 0) {
	    gssapi_krb5_set_error_string();
	    *minor_status = ret;
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    gss_release_buffer(minor_status, output_message_buffer);
	    return GSS_S_FAILURE;
	}
	assert(sizeof(*token) + cipher.length == wrapped_len);
	token->RRC[0] = (rrc >> 8) & 0xFF;  
	token->RRC[1] = (rrc >> 0) & 0xFF;

	ret = rrc_rotate(cipher.data, cipher.length, rrc, FALSE);
	if (ret != 0) {
	    gssapi_krb5_set_error_string();
	    *minor_status = ret;
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    gss_release_buffer(minor_status, output_message_buffer);
	    return GSS_S_FAILURE;
	}
	memcpy(p, cipher.data, cipher.length);
	krb5_data_free(&cipher);
    } else {
	char *buf;
	Checksum cksum;

	buf = malloc(input_message_buffer->length + sizeof(*token));
	if (buf == NULL) {
	    *minor_status = ENOMEM;
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    gss_release_buffer(minor_status, output_message_buffer);
	    return GSS_S_FAILURE;
	}
	memcpy(buf, input_message_buffer->value, input_message_buffer->length);
	memcpy(buf + input_message_buffer->length, token, sizeof(*token));

	ret = krb5_create_checksum(gssapi_krb5_context, crypto,
				   usage, 0, buf, 
				   input_message_buffer->length +
					sizeof(*token), 
				   &cksum);
	if (ret != 0) {
	    gssapi_krb5_set_error_string();
	    *minor_status = ret;
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    gss_release_buffer(minor_status, output_message_buffer);
	    free(buf);
	    return GSS_S_FAILURE;
	}

	free(buf);

	assert(cksum.checksum.length == cksumsize);
	token->EC[0] =  (cksum.checksum.length >> 8) & 0xFF;
	token->EC[1] =  (cksum.checksum.length >> 0) & 0xFF;
	token->RRC[0] = (rrc >> 8) & 0xFF;  
	token->RRC[1] = (rrc >> 0) & 0xFF;

	p += sizeof(*token);
	memcpy(p, input_message_buffer->value, input_message_buffer->length);
	memcpy(p + input_message_buffer->length,
	       cksum.checksum.data, cksum.checksum.length);

	ret = rrc_rotate(p,
	    input_message_buffer->length + cksum.checksum.length, rrc, FALSE);
	if (ret != 0) {
	    gssapi_krb5_set_error_string();
	    *minor_status = ret;
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    gss_release_buffer(minor_status, output_message_buffer);
	    free_Checksum(&cksum);
	    return GSS_S_FAILURE;
	}
	free_Checksum(&cksum);
    }

    krb5_crypto_destroy(gssapi_krb5_context, crypto);

    if (conf_state != NULL) {
	*conf_state = conf_req_flag;
    }

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32 _gssapi_unwrap_cfx(OM_uint32 *minor_status,
			     const gss_ctx_id_t context_handle,
			     const gss_buffer_t input_message_buffer,
			     gss_buffer_t output_message_buffer,
			     int *conf_state,
			     gss_qop_t *qop_state,
			     krb5_keyblock *key)
{
    krb5_crypto crypto;
    gss_cfx_wrap_token token;
    u_char token_flags;
    krb5_error_code ret;
    unsigned usage;
    krb5_data data;
    u_int16_t ec, rrc;
    OM_uint32 seq_number_lo, seq_number_hi;
    size_t len;
    u_char *p;

    *minor_status = 0;

    if (input_message_buffer->length < sizeof(*token)) {
	return GSS_S_DEFECTIVE_TOKEN;
    }

    p = input_message_buffer->value;

    token = (gss_cfx_wrap_token)p;

    if (token->TOK_ID[0] != 0x05 || token->TOK_ID[1] != 0x04) {
	return GSS_S_DEFECTIVE_TOKEN;
    }

    /* Ignore unknown flags */
    token_flags = token->Flags &
	(CFXSentByAcceptor | CFXSealed | CFXAcceptorSubkey);

    if (token_flags & CFXSentByAcceptor) {
	if ((context_handle->more_flags & LOCAL) == 0)
	    return GSS_S_DEFECTIVE_TOKEN;
    }

    if (context_handle->more_flags & ACCEPTOR_SUBKEY) {
	if ((token_flags & CFXAcceptorSubkey) == 0)
	    return GSS_S_DEFECTIVE_TOKEN;
    } else {
	if (token_flags & CFXAcceptorSubkey)
	    return GSS_S_DEFECTIVE_TOKEN;
    }

    if (token->Filler != 0xFF) {
	return GSS_S_DEFECTIVE_TOKEN;
    }

    if (conf_state != NULL) {
	*conf_state = (token_flags & CFXSealed) ? 1 : 0;
    }

    ec  = (token->EC[0]  << 8) | token->EC[1];
    rrc = (token->RRC[0] << 8) | token->RRC[1];

    /*
     * Check sequence number
     */
    gssapi_decode_be_om_uint32(&token->SND_SEQ[0], &seq_number_hi);
    gssapi_decode_be_om_uint32(&token->SND_SEQ[4], &seq_number_lo);
    if (seq_number_hi) {
	/* no support for 64-bit sequence numbers */
	*minor_status = ERANGE;
	return GSS_S_UNSEQ_TOKEN;
    }

    HEIMDAL_MUTEX_lock(&context_handle->ctx_id_mutex);
    ret = _gssapi_msg_order_check(context_handle->order, seq_number_lo);
    if (ret != 0) {
	*minor_status = 0;
	HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);
	gss_release_buffer(minor_status, output_message_buffer);
	return ret;
    }
    HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);

    /*
     * Decrypt and/or verify checksum
     */
    ret = krb5_crypto_init(gssapi_krb5_context, key, 0, &crypto);
    if (ret != 0) {
	gssapi_krb5_set_error_string();
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    if (context_handle->more_flags & LOCAL) {
	usage = KRB5_KU_USAGE_ACCEPTOR_SEAL;
    } else {
	usage = KRB5_KU_USAGE_INITIATOR_SEAL;
    }

    p += sizeof(*token);
    len = input_message_buffer->length;
    len -= (p - (u_char *)input_message_buffer->value);

    /* Rotate by RRC; bogus to do this in-place XXX */
    *minor_status = rrc_rotate(p, len, rrc, TRUE);
    if (*minor_status != 0) {
	krb5_crypto_destroy(gssapi_krb5_context, crypto);
	return GSS_S_FAILURE;
    }

    if (token_flags & CFXSealed) {
	ret = krb5_decrypt(gssapi_krb5_context, crypto, usage,
	    p, len, &data);
	if (ret != 0) {
	    gssapi_krb5_set_error_string();
	    *minor_status = ret;
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    return GSS_S_BAD_MIC;
	}

	/* Check that there is room for the pad and token header */
	if (data.length < ec + sizeof(*token)) {
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    krb5_data_free(&data);
	    return GSS_S_DEFECTIVE_TOKEN;
	}
	p = data.data;
	p += data.length - sizeof(*token);

	/* RRC is unprotected; don't modify input buffer */
	((gss_cfx_wrap_token)p)->RRC[0] = token->RRC[0];
	((gss_cfx_wrap_token)p)->RRC[1] = token->RRC[1];

	/* Check the integrity of the header */
	if (memcmp(p, token, sizeof(*token)) != 0) {
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    krb5_data_free(&data);
	    return GSS_S_BAD_MIC;
	}

	output_message_buffer->value = data.data;
	output_message_buffer->length = data.length - ec - sizeof(*token);
    } else {
	Checksum cksum;

	/* Determine checksum type */
	ret = krb5_crypto_get_checksum_type(gssapi_krb5_context,
					    crypto, &cksum.cksumtype);
	if (ret != 0) {
	    gssapi_krb5_set_error_string();
	    *minor_status = ret;
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    return GSS_S_FAILURE;
	}

	cksum.checksum.length = ec;

	/* Check we have at least as much data as the checksum */
	if (len < cksum.checksum.length) {
	    *minor_status = ERANGE;
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    return GSS_S_BAD_MIC;
	}

	/* Length now is of the plaintext only, no checksum */
	len -= cksum.checksum.length;
	cksum.checksum.data = p + len;

	output_message_buffer->length = len; /* for later */
	output_message_buffer->value = malloc(len + sizeof(*token));
	if (output_message_buffer->value == NULL) {
	    *minor_status = ENOMEM;
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    return GSS_S_FAILURE;
	}

	/* Checksum is over (plaintext-data | "header") */
	memcpy(output_message_buffer->value, p, len);
	memcpy((u_char *)output_message_buffer->value + len, 
	       token, sizeof(*token));

	/* EC is not included in checksum calculation */
	token = (gss_cfx_wrap_token)((u_char *)output_message_buffer->value +
				     len);
	token->EC[0]  = 0;
	token->EC[1]  = 0;
	token->RRC[0] = 0;
	token->RRC[1] = 0;

	ret = krb5_verify_checksum(gssapi_krb5_context, crypto,
				   usage,
				   output_message_buffer->value,
				   len + sizeof(*token),
				   &cksum);
	if (ret != 0) {
	    gssapi_krb5_set_error_string();
	    *minor_status = ret;
	    krb5_crypto_destroy(gssapi_krb5_context, crypto);
	    gss_release_buffer(minor_status, output_message_buffer);
	    return GSS_S_BAD_MIC;
	}
    }

    krb5_crypto_destroy(gssapi_krb5_context, crypto);

    if (qop_state != NULL) {
	*qop_state = GSS_C_QOP_DEFAULT;
    }

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32 _gssapi_mic_cfx(OM_uint32 *minor_status,
			  const gss_ctx_id_t context_handle,
			  gss_qop_t qop_req,
			  const gss_buffer_t message_buffer,
			  gss_buffer_t message_token,
			  krb5_keyblock *key)
{
    krb5_crypto crypto;
    gss_cfx_mic_token token;
    krb5_error_code ret;
    unsigned usage;
    Checksum cksum;
    u_char *buf;
    size_t len;
    OM_uint32 seq_number;

    ret = krb5_crypto_init(gssapi_krb5_context, key, 0, &crypto);
    if (ret != 0) {
	gssapi_krb5_set_error_string();
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    len = message_buffer->length + sizeof(*token);
    buf = malloc(len);
    if (buf == NULL) {
	*minor_status = ENOMEM;
	krb5_crypto_destroy(gssapi_krb5_context, crypto);
	return GSS_S_FAILURE;
    }

    memcpy(buf, message_buffer->value, message_buffer->length);

    token = (gss_cfx_mic_token)(buf + message_buffer->length);
    token->TOK_ID[0] = 0x04;
    token->TOK_ID[1] = 0x04;
    token->Flags = 0;
    if ((context_handle->more_flags & LOCAL) == 0)
	token->Flags |= CFXSentByAcceptor;
    if (context_handle->more_flags & ACCEPTOR_SUBKEY)
	token->Flags |= CFXAcceptorSubkey;
    memset(token->Filler, 0xFF, 5);

    HEIMDAL_MUTEX_lock(&context_handle->ctx_id_mutex);
    krb5_auth_con_getlocalseqnumber(gssapi_krb5_context,
				    context_handle->auth_context,
				    &seq_number);
    gssapi_encode_be_om_uint32(0,          &token->SND_SEQ[0]);
    gssapi_encode_be_om_uint32(seq_number, &token->SND_SEQ[4]);
    krb5_auth_con_setlocalseqnumber(gssapi_krb5_context,
				    context_handle->auth_context,
				    ++seq_number);
    HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);

    if (context_handle->more_flags & LOCAL) {
	usage = KRB5_KU_USAGE_INITIATOR_SIGN;
    } else {
	usage = KRB5_KU_USAGE_ACCEPTOR_SIGN;
    }

    ret = krb5_create_checksum(gssapi_krb5_context, crypto,
	usage, 0, buf, len, &cksum);
    if (ret != 0) {
	gssapi_krb5_set_error_string();
	*minor_status = ret;
	krb5_crypto_destroy(gssapi_krb5_context, crypto);
	free(buf);
	return GSS_S_FAILURE;
    }
    krb5_crypto_destroy(gssapi_krb5_context, crypto);

    /* Determine MIC length */
    message_token->length = sizeof(*token) + cksum.checksum.length;
    message_token->value = malloc(message_token->length);
    if (message_token->value == NULL) {
	*minor_status = ENOMEM;
	free_Checksum(&cksum);
	free(buf);
	return GSS_S_FAILURE;
    }

    /* Token is { "header" | get_mic("header" | plaintext-data) } */
    memcpy(message_token->value, token, sizeof(*token));
    memcpy((u_char *)message_token->value + sizeof(*token),
	   cksum.checksum.data, cksum.checksum.length);

    free_Checksum(&cksum);
    free(buf);

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32 _gssapi_verify_mic_cfx(OM_uint32 *minor_status,
				 const gss_ctx_id_t context_handle,
				 const gss_buffer_t message_buffer,
				 const gss_buffer_t token_buffer,
				 gss_qop_t *qop_state,
				 krb5_keyblock *key)
{
    krb5_crypto crypto;
    gss_cfx_mic_token token;
    u_char token_flags;
    krb5_error_code ret;
    unsigned usage;
    OM_uint32 seq_number_lo, seq_number_hi;
    u_char *buf, *p;
    Checksum cksum;

    *minor_status = 0;

    if (token_buffer->length < sizeof(*token)) {
	return GSS_S_DEFECTIVE_TOKEN;
    }

    p = token_buffer->value;

    token = (gss_cfx_mic_token)p;

    if (token->TOK_ID[0] != 0x04 || token->TOK_ID[1] != 0x04) {
	return GSS_S_DEFECTIVE_TOKEN;
    }

    /* Ignore unknown flags */
    token_flags = token->Flags & (CFXSentByAcceptor | CFXAcceptorSubkey);

    if (token_flags & CFXSentByAcceptor) {
	if ((context_handle->more_flags & LOCAL) == 0)
	    return GSS_S_DEFECTIVE_TOKEN;
    }
    if (context_handle->more_flags & ACCEPTOR_SUBKEY) {
	if ((token_flags & CFXAcceptorSubkey) == 0)
	    return GSS_S_DEFECTIVE_TOKEN;
    } else {
	if (token_flags & CFXAcceptorSubkey)
	    return GSS_S_DEFECTIVE_TOKEN;
    }

    if (memcmp(token->Filler, "\xff\xff\xff\xff\xff", 5) != 0) {
	return GSS_S_DEFECTIVE_TOKEN;
    }

    /*
     * Check sequence number
     */
    gssapi_decode_be_om_uint32(&token->SND_SEQ[0], &seq_number_hi);
    gssapi_decode_be_om_uint32(&token->SND_SEQ[4], &seq_number_lo);
    if (seq_number_hi) {
	*minor_status = ERANGE;
	return GSS_S_UNSEQ_TOKEN;
    }

    HEIMDAL_MUTEX_lock(&context_handle->ctx_id_mutex);
    ret = _gssapi_msg_order_check(context_handle->order, seq_number_lo);
    if (ret != 0) {
	*minor_status = 0;
	HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);
	return ret;
    }
    HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);

    /*
     * Verify checksum
     */
    ret = krb5_crypto_init(gssapi_krb5_context, key, 0, &crypto);
    if (ret != 0) {
	gssapi_krb5_set_error_string();
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_crypto_get_checksum_type(gssapi_krb5_context, crypto,
					&cksum.cksumtype);
    if (ret != 0) {
	gssapi_krb5_set_error_string();
	*minor_status = ret;
	krb5_crypto_destroy(gssapi_krb5_context, crypto);
	return GSS_S_FAILURE;
    }

    cksum.checksum.data = p + sizeof(*token);
    cksum.checksum.length = token_buffer->length - sizeof(*token);

    if (context_handle->more_flags & LOCAL) {
	usage = KRB5_KU_USAGE_ACCEPTOR_SIGN;
    } else {
	usage = KRB5_KU_USAGE_INITIATOR_SIGN;
    }

    buf = malloc(message_buffer->length + sizeof(*token));
    if (buf == NULL) {
	*minor_status = ENOMEM;
	krb5_crypto_destroy(gssapi_krb5_context, crypto);
	return GSS_S_FAILURE;
    }
    memcpy(buf, message_buffer->value, message_buffer->length);
    memcpy(buf + message_buffer->length, token, sizeof(*token));

    ret = krb5_verify_checksum(gssapi_krb5_context, crypto,
			       usage,
			       buf,
			       sizeof(*token) + message_buffer->length,
			       &cksum);
    if (ret != 0) {
	gssapi_krb5_set_error_string();
	*minor_status = ret;
	krb5_crypto_destroy(gssapi_krb5_context, crypto);
	free(buf);
	return GSS_S_BAD_MIC;
    }

    free(buf);

    if (qop_state != NULL) {
	*qop_state = GSS_C_QOP_DEFAULT;
    }

    return GSS_S_COMPLETE;
}
