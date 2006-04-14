/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska Högskolan
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

RCSID("$KTH: wrap.c,v 1.31 2005/01/05 02:52:12 lukeh Exp $");

OM_uint32
gss_krb5_get_subkey(const gss_ctx_id_t context_handle,
		    krb5_keyblock **key)
{
    krb5_keyblock *skey = NULL;

    HEIMDAL_MUTEX_lock(&context_handle->ctx_id_mutex);
    if (context_handle->more_flags & LOCAL) {
	krb5_auth_con_getremotesubkey(gssapi_krb5_context,
				      context_handle->auth_context, 
				      &skey);
    } else {
	krb5_auth_con_getlocalsubkey(gssapi_krb5_context,
				     context_handle->auth_context, 
				     &skey);
    }
    /*
     * Only use the initiator subkey or ticket session key if
     * an acceptor subkey was not required.
     */
    if (skey == NULL &&
	(context_handle->more_flags & ACCEPTOR_SUBKEY) == 0) {
	if (context_handle->more_flags & LOCAL) {
	    krb5_auth_con_getlocalsubkey(gssapi_krb5_context,
					 context_handle->auth_context,
					 &skey);
	} else {
	    krb5_auth_con_getremotesubkey(gssapi_krb5_context,
					  context_handle->auth_context,
					  &skey);
	}
	if(skey == NULL)
	    krb5_auth_con_getkey(gssapi_krb5_context,
				 context_handle->auth_context, 
				 &skey);
    }
    HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);
    if(skey == NULL)
	return GSS_KRB5_S_KG_NO_SUBKEY; /* XXX */
    *key = skey;
    return 0;
}

static OM_uint32
sub_wrap_size (
            OM_uint32 req_output_size,
            OM_uint32 * max_input_size,
	    int blocksize,
	    int extrasize
           )
{
    size_t len, total_len; 

    len = 8 + req_output_size + blocksize + extrasize;

    gssapi_krb5_encap_length(len, &len, &total_len, GSS_KRB5_MECHANISM);

    total_len -= req_output_size; /* token length */
    if (total_len < req_output_size) {
        *max_input_size = (req_output_size - total_len);
        (*max_input_size) &= (~(OM_uint32)(blocksize - 1));
    } else {
        *max_input_size = 0;
    }
    return GSS_S_COMPLETE;
}

OM_uint32
gss_wrap_size_limit (
            OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            int conf_req_flag,
            gss_qop_t qop_req,
            OM_uint32 req_output_size,
            OM_uint32 * max_input_size
           )
{
  krb5_keyblock *key;
  OM_uint32 ret;
  krb5_keytype keytype;

  ret = gss_krb5_get_subkey(context_handle, &key);
  if (ret) {
      gssapi_krb5_set_error_string ();
      *minor_status = ret;
      return GSS_S_FAILURE;
  }
  krb5_enctype_to_keytype (gssapi_krb5_context, key->keytype, &keytype);

  switch (keytype) {
  case KEYTYPE_DES :
  case KEYTYPE_ARCFOUR:
  case KEYTYPE_ARCFOUR_56:
      ret = sub_wrap_size(req_output_size, max_input_size, 8, 22);
      break;
  case KEYTYPE_DES3 :
      ret = sub_wrap_size(req_output_size, max_input_size, 8, 34);
      break;
  default :
      ret = _gssapi_wrap_size_cfx(minor_status, context_handle, 
				  conf_req_flag, qop_req, 
				  req_output_size, max_input_size, key);
      break;
  }
  krb5_free_keyblock (gssapi_krb5_context, key);
  *minor_status = 0;
  return ret;
}

static OM_uint32
wrap_des
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            int conf_req_flag,
            gss_qop_t qop_req,
            const gss_buffer_t input_message_buffer,
            int * conf_state,
            gss_buffer_t output_message_buffer,
	    krb5_keyblock *key
           )
{
  u_char *p;
  MD5_CTX md5;
  u_char hash[16];
  DES_key_schedule schedule;
  DES_cblock deskey;
  DES_cblock zero;
  int i;
  int32_t seq_number;
  size_t len, total_len, padlength, datalen;

  padlength = 8 - (input_message_buffer->length % 8);
  datalen = input_message_buffer->length + padlength + 8;
  len = datalen + 22;
  gssapi_krb5_encap_length (len, &len, &total_len, GSS_KRB5_MECHANISM);

  output_message_buffer->length = total_len;
  output_message_buffer->value  = malloc (total_len);
  if (output_message_buffer->value == NULL) {
    *minor_status = ENOMEM;
    return GSS_S_FAILURE;
  }

  p = gssapi_krb5_make_header(output_message_buffer->value,
			      len,
			      "\x02\x01", /* TOK_ID */
			      GSS_KRB5_MECHANISM);

  /* SGN_ALG */
  memcpy (p, "\x00\x00", 2);
  p += 2;
  /* SEAL_ALG */
  if(conf_req_flag)
      memcpy (p, "\x00\x00", 2);
  else
      memcpy (p, "\xff\xff", 2);
  p += 2;
  /* Filler */
  memcpy (p, "\xff\xff", 2);
  p += 2;

  /* fill in later */
  memset (p, 0, 16);
  p += 16;

  /* confounder + data + pad */
  krb5_generate_random_block(p, 8);
  memcpy (p + 8, input_message_buffer->value,
	  input_message_buffer->length);
  memset (p + 8 + input_message_buffer->length, padlength, padlength);

  /* checksum */
  MD5_Init (&md5);
  MD5_Update (&md5, p - 24, 8);
  MD5_Update (&md5, p, datalen);
  MD5_Final (hash, &md5);

  memset (&zero, 0, sizeof(zero));
  memcpy (&deskey, key->keyvalue.data, sizeof(deskey));
  DES_set_key (&deskey, &schedule);
  DES_cbc_cksum ((void *)hash, (void *)hash, sizeof(hash),
		 &schedule, &zero);
  memcpy (p - 8, hash, 8);

  /* sequence number */
  HEIMDAL_MUTEX_lock(&context_handle->ctx_id_mutex);
  krb5_auth_con_getlocalseqnumber (gssapi_krb5_context,
			       context_handle->auth_context,
			       &seq_number);

  p -= 16;
  p[0] = (seq_number >> 0)  & 0xFF;
  p[1] = (seq_number >> 8)  & 0xFF;
  p[2] = (seq_number >> 16) & 0xFF;
  p[3] = (seq_number >> 24) & 0xFF;
  memset (p + 4,
	  (context_handle->more_flags & LOCAL) ? 0 : 0xFF,
	  4);

  DES_set_key (&deskey, &schedule);
  DES_cbc_encrypt ((void *)p, (void *)p, 8,
		   &schedule, (DES_cblock *)(p + 8), DES_ENCRYPT);

  krb5_auth_con_setlocalseqnumber (gssapi_krb5_context,
			       context_handle->auth_context,
			       ++seq_number);
  HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);

  /* encrypt the data */
  p += 16;

  if(conf_req_flag) {
      memcpy (&deskey, key->keyvalue.data, sizeof(deskey));

      for (i = 0; i < sizeof(deskey); ++i)
	  deskey[i] ^= 0xf0;
      DES_set_key (&deskey, &schedule);
      memset (&zero, 0, sizeof(zero));
      DES_cbc_encrypt ((void *)p,
		       (void *)p,
		       datalen,
		       &schedule,
		       &zero,
		       DES_ENCRYPT);
  }
  memset (deskey, 0, sizeof(deskey));
  memset (&schedule, 0, sizeof(schedule));

  if(conf_state != NULL)
      *conf_state = conf_req_flag;
  *minor_status = 0;
  return GSS_S_COMPLETE;
}

static OM_uint32
wrap_des3
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            int conf_req_flag,
            gss_qop_t qop_req,
            const gss_buffer_t input_message_buffer,
            int * conf_state,
            gss_buffer_t output_message_buffer,
	    krb5_keyblock *key
           )
{
  u_char *p;
  u_char seq[8];
  int32_t seq_number;
  size_t len, total_len, padlength, datalen;
  u_int32_t ret;
  krb5_crypto crypto;
  Checksum cksum;
  krb5_data encdata;

  padlength = 8 - (input_message_buffer->length % 8);
  datalen = input_message_buffer->length + padlength + 8;
  len = datalen + 34;
  gssapi_krb5_encap_length (len, &len, &total_len, GSS_KRB5_MECHANISM);

  output_message_buffer->length = total_len;
  output_message_buffer->value  = malloc (total_len);
  if (output_message_buffer->value == NULL) {
    *minor_status = ENOMEM;
    return GSS_S_FAILURE;
  }

  p = gssapi_krb5_make_header(output_message_buffer->value,
			      len,
			      "\x02\x01", /* TOK_ID */
			      GSS_KRB5_MECHANISM); 

  /* SGN_ALG */
  memcpy (p, "\x04\x00", 2);	/* HMAC SHA1 DES3-KD */
  p += 2;
  /* SEAL_ALG */
  if(conf_req_flag)
      memcpy (p, "\x02\x00", 2); /* DES3-KD */
  else
      memcpy (p, "\xff\xff", 2);
  p += 2;
  /* Filler */
  memcpy (p, "\xff\xff", 2);
  p += 2;

  /* calculate checksum (the above + confounder + data + pad) */

  memcpy (p + 20, p - 8, 8);
  krb5_generate_random_block(p + 28, 8);
  memcpy (p + 28 + 8, input_message_buffer->value,
	  input_message_buffer->length);
  memset (p + 28 + 8 + input_message_buffer->length, padlength, padlength);

  ret = krb5_crypto_init(gssapi_krb5_context, key, 0, &crypto);
  if (ret) {
      gssapi_krb5_set_error_string ();
      free (output_message_buffer->value);
      *minor_status = ret;
      return GSS_S_FAILURE;
  }

  ret = krb5_create_checksum (gssapi_krb5_context,
			      crypto,
			      KRB5_KU_USAGE_SIGN,
			      0,
			      p + 20,
			      datalen + 8,
			      &cksum);
  krb5_crypto_destroy (gssapi_krb5_context, crypto);
  if (ret) {
      gssapi_krb5_set_error_string ();
      free (output_message_buffer->value);
      *minor_status = ret;
      return GSS_S_FAILURE;
  }

  /* zero out SND_SEQ + SGN_CKSUM in case */
  memset (p, 0, 28);

  memcpy (p + 8, cksum.checksum.data, cksum.checksum.length);
  free_Checksum (&cksum);

  HEIMDAL_MUTEX_lock(&context_handle->ctx_id_mutex);
  /* sequence number */
  krb5_auth_con_getlocalseqnumber (gssapi_krb5_context,
			       context_handle->auth_context,
			       &seq_number);

  seq[0] = (seq_number >> 0)  & 0xFF;
  seq[1] = (seq_number >> 8)  & 0xFF;
  seq[2] = (seq_number >> 16) & 0xFF;
  seq[3] = (seq_number >> 24) & 0xFF;
  memset (seq + 4,
	  (context_handle->more_flags & LOCAL) ? 0 : 0xFF,
	  4);


  ret = krb5_crypto_init(gssapi_krb5_context, key, ETYPE_DES3_CBC_NONE,
			 &crypto);
  if (ret) {
      free (output_message_buffer->value);
      *minor_status = ret;
      return GSS_S_FAILURE;
  }

  {
      DES_cblock ivec;

      memcpy (&ivec, p + 8, 8);
      ret = krb5_encrypt_ivec (gssapi_krb5_context,
			       crypto,
			       KRB5_KU_USAGE_SEQ,
			       seq, 8, &encdata,
			       &ivec);
  }
  krb5_crypto_destroy (gssapi_krb5_context, crypto);
  if (ret) {
      gssapi_krb5_set_error_string ();
      free (output_message_buffer->value);
      *minor_status = ret;
      return GSS_S_FAILURE;
  }
  
  assert (encdata.length == 8);

  memcpy (p, encdata.data, encdata.length);
  krb5_data_free (&encdata);

  krb5_auth_con_setlocalseqnumber (gssapi_krb5_context,
			       context_handle->auth_context,
			       ++seq_number);
  HEIMDAL_MUTEX_unlock(&context_handle->ctx_id_mutex);

  /* encrypt the data */
  p += 28;

  if(conf_req_flag) {
      krb5_data tmp;

      ret = krb5_crypto_init(gssapi_krb5_context, key,
			     ETYPE_DES3_CBC_NONE, &crypto);
      if (ret) {
	  gssapi_krb5_set_error_string ();
	  free (output_message_buffer->value);
	  *minor_status = ret;
	  return GSS_S_FAILURE;
      }
      ret = krb5_encrypt(gssapi_krb5_context, crypto, KRB5_KU_USAGE_SEAL,
			 p, datalen, &tmp);
      krb5_crypto_destroy(gssapi_krb5_context, crypto);
      if (ret) {
	  gssapi_krb5_set_error_string ();
	  free (output_message_buffer->value);
	  *minor_status = ret;
	  return GSS_S_FAILURE;
      }
      assert (tmp.length == datalen);

      memcpy (p, tmp.data, datalen);
      krb5_data_free(&tmp);
  }
  if(conf_state != NULL)
      *conf_state = conf_req_flag;
  *minor_status = 0;
  return GSS_S_COMPLETE;
}

OM_uint32 gss_wrap
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            int conf_req_flag,
            gss_qop_t qop_req,
            const gss_buffer_t input_message_buffer,
            int * conf_state,
            gss_buffer_t output_message_buffer
           )
{
  krb5_keyblock *key;
  OM_uint32 ret;
  krb5_keytype keytype;

  ret = gss_krb5_get_subkey(context_handle, &key);
  if (ret) {
      gssapi_krb5_set_error_string ();
      *minor_status = ret;
      return GSS_S_FAILURE;
  }
  krb5_enctype_to_keytype (gssapi_krb5_context, key->keytype, &keytype);

  switch (keytype) {
  case KEYTYPE_DES :
      ret = wrap_des (minor_status, context_handle, conf_req_flag,
		      qop_req, input_message_buffer, conf_state,
		      output_message_buffer, key);
      break;
  case KEYTYPE_DES3 :
      ret = wrap_des3 (minor_status, context_handle, conf_req_flag,
		       qop_req, input_message_buffer, conf_state,
		       output_message_buffer, key);
      break;
  case KEYTYPE_ARCFOUR:
  case KEYTYPE_ARCFOUR_56:
      ret = _gssapi_wrap_arcfour (minor_status, context_handle, conf_req_flag,
				  qop_req, input_message_buffer, conf_state,
				  output_message_buffer, key);
      break;
  default :
      ret = _gssapi_wrap_cfx (minor_status, context_handle, conf_req_flag,
			      qop_req, input_message_buffer, conf_state,
			      output_message_buffer, key);
      break;
  }
  krb5_free_keyblock (gssapi_krb5_context, key);
  return ret;
}
