/*	$OpenBSD: policy.c,v 1.6 2000/02/01 02:46:18 niklas Exp $	*/
/*	$EOM: policy.c,v 1.14 2000/01/31 22:33:48 niklas Exp $ */

/*
 * Copyright (c) 1999, 2000 Angelos D. Keromytis.  All rights reserved.
 * Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
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
 *	This product includes software developed by Ericsson Radio Systems.
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
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <regex.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <keynote.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "sysdep.h"

#include "app.h"
#include "conf.h"
#include "connection.h"
#include "cookie.h"
#include "doi.h"
#include "dyn.h"
#include "exchange.h"
#include "init.h"
#include "ipsec.h"
#include "isakmp_doi.h"
#include "math_group.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"
#include "udp.h"
#include "log.h"
#include "message.h"
#include "ui.h"
#include "util.h"
#include "policy.h"

#ifndef POLICY_FILE_DEFAULT
#define POLICY_FILE_DEFAULT "/etc/isakmpd/isakmpd.policy"
#endif /* POLICY_FILE_DEFAULT */

#if defined (HAVE_DLOPEN) && !defined (USE_KEYNOTE)

void *libkeynote = 0;

/*
 * These prototypes matches OpenBSD keynote.h 1.6.  If you use
 * a different version than that, you are on your own.
 */
int *lk_keynote_errno;
int (*lk_kn_add_action) (int, char *, char *, int);
int (*lk_kn_add_assertion) (int, char *, int, int);
int (*lk_kn_add_authorizer) (int, char *);
int (*lk_kn_close) (int);
int (*lk_kn_do_query) (int, char **, int);
char *(*lk_kn_encode_key) (struct keynote_deckey *, int, int, int);
int (*lk_kn_init) (void);
char **(*lk_kn_read_asserts) (char *, int, int *);
int (*lk_kn_remove_authorizer) (int, char *);
#define SYMENTRY(x) { SYM, SYM (x), (void **)&lk_ ## x }

static struct dynload_script libkeynote_script[] = {
  { LOAD, "libc.so", &libkeynote },
  { LOAD, "libcrypto.so", &libkeynote },
  { LOAD, "libm.so", &libkeynote },
  { LOAD, "libkeynote.so", &libkeynote },
  SYMENTRY (keynote_errno),
  SYMENTRY (kn_add_action),
  SYMENTRY (kn_add_assertion),
  SYMENTRY (kn_add_authorizer),
  SYMENTRY (kn_close),
  SYMENTRY (kn_do_query),
  SYMENTRY (kn_encode_key),
  SYMENTRY (kn_init),
  SYMENTRY (kn_read_asserts),
  SYMENTRY (kn_remove_authorizer),
  { EOS }
};
#endif

int keynote_sessid = -1;

struct exchange *policy_exchange = 0;
struct sa *policy_sa = 0;
struct sa *policy_isakmp_sa = 0;

/*
 * Adaptation of Vixie's inet_ntop4 ()
 */
static const char *
my_inet_ntop4 (const in_addr_t *src, char *dst, size_t size, int normalize)
{
  static const char fmt[] = "%03u.%03u.%03u.%03u";
  char tmp[sizeof "255.255.255.255"];
  in_addr_t src2;

  if (normalize)
    src2 = ntohl (*src);
  else
    src2 = *src;

  if (sprintf (tmp, fmt, ((u_int8_t *) &src2)[0], ((u_int8_t *) &src2)[1],
	       ((u_int8_t *) &src2)[2], ((u_int8_t *) &src2)[3]) > size)
    {
      errno = ENOSPC;
      return 0;
    }
  strcpy (dst, tmp);
  return dst;
}

static char *
policy_callback (char *name)
{
  struct proto *proto;

  u_int8_t *attr, *value, *id;
  struct sockaddr_in *sin;
  struct ipsec_exch *ie;
  int fmt, lifetype = 0;
  in_addr_t net, subnet;
  u_int16_t len, type;
  size_t id_sz;
  time_t tt;
  static char mytimeofday[15];

  /* We use all these as a cache.  */
  static char *esp_present, *ah_present, *comp_present;
  static char *ah_hash_alg, *ah_auth_alg, *esp_auth_alg, *esp_enc_alg;
  static char *comp_alg, ah_life_kbytes[32], ah_life_seconds[32];
  static char esp_life_kbytes[32], esp_life_seconds[32], comp_life_kbytes[32];
  static char comp_life_seconds[32], *ah_encapsulation, *esp_encapsulation;
  static char *comp_encapsulation, ah_key_length[32], esp_key_length[32];
  static char ah_key_rounds[32], esp_key_rounds[32], comp_dict_size[32];
  static char comp_private_alg[32], *remote_filter_type, *local_filter_type;
  static char remote_filter_addr_upper[64], remote_filter_addr_lower[64];
  static char local_filter_addr_upper[64], local_filter_addr_lower[64];
  static char ah_group_desc[32], esp_group_desc[32], comp_group_desc[32];
  static char remote_ike_address[64], local_ike_address[64];
  static char *remote_id_type, remote_id_addr_upper[64];
  static char remote_id_addr_lower[64], *remote_id_proto, remote_id_port[32];
  static char remote_filter_port[32], local_filter_port[32];
  static char *remote_filter_proto, *local_filter_proto, *pfs;

  /* Allocated.  */
  static char *remote_filter = 0, *local_filter = 0, *remote_id = 0;

  static int dirty = 1;

  /* We only need to set dirty at initialization time really.  */
  if (strcmp (name, KEYNOTE_CALLBACK_CLEANUP) == 0
      || strcmp (name, KEYNOTE_CALLBACK_INITIALIZE) == 0)
    {
      esp_present = ah_present = comp_present = pfs = "no";
      ah_hash_alg = ah_auth_alg = "";
      esp_auth_alg = esp_enc_alg = comp_alg = ah_encapsulation = "";
      esp_encapsulation = comp_encapsulation = remote_filter_type = "";
      local_filter_type = remote_id_type = "";
      remote_filter_proto = local_filter_proto = remote_id_proto = "";

      if (remote_filter != 0)
        {
	  free (remote_filter);
	  remote_filter = 0;
	}

      if (local_filter != 0)
        {
	  free (local_filter);
	  local_filter = 0;
	}

      if (remote_id != 0)
        {
	  free (remote_id);
	  remote_id = 0;
	}

      memset (remote_ike_address, 0, sizeof remote_ike_address);
      memset (local_ike_address, 0, sizeof local_ike_address);
      memset (ah_life_kbytes, 0, sizeof ah_life_kbytes);
      memset (ah_life_seconds, 0, sizeof ah_life_seconds);
      memset (esp_life_kbytes, 0, sizeof esp_life_kbytes);
      memset (esp_life_seconds, 0, sizeof esp_life_seconds);
      memset (comp_life_kbytes, 0, sizeof comp_life_kbytes);
      memset (comp_life_seconds, 0, sizeof comp_life_seconds);
      memset (ah_key_length, 0, sizeof ah_key_length);
      memset (ah_key_rounds, 0, sizeof ah_key_rounds);
      memset (esp_key_length, 0, sizeof esp_key_length);
      memset (esp_key_rounds, 0, sizeof esp_key_rounds);
      memset (comp_dict_size, 0, sizeof comp_dict_size);
      memset (comp_private_alg, 0, sizeof comp_private_alg);
      memset (remote_filter_addr_upper, 0, sizeof remote_filter_addr_upper);
      memset (remote_filter_addr_lower, 0, sizeof remote_filter_addr_lower);
      memset (local_filter_addr_upper, 0, sizeof local_filter_addr_upper);
      memset (local_filter_addr_lower, 0, sizeof local_filter_addr_lower);
      memset (remote_id_addr_upper, 0, sizeof remote_id_addr_upper);
      memset (remote_id_addr_lower, 0, sizeof remote_id_addr_lower);
      memset (ah_group_desc, 0, sizeof ah_group_desc);
      memset (esp_group_desc, 0, sizeof esp_group_desc);
      memset (remote_id_port, 0, sizeof remote_id_port);
      memset (remote_filter_port, 0, sizeof remote_filter_port);
      memset (local_filter_port, 0, sizeof local_filter_port);

      dirty = 1;
      return "";
    }

  /*
   * If dirty is set, this is the first request for an attribute, so
   * populate our value cache.
   */
  if (dirty)
    {
      ie = policy_exchange->data;

      if (ie->pfs)
	pfs = "yes";

      for (proto = TAILQ_FIRST (&policy_sa->protos); proto;
	   proto = TAILQ_NEXT (proto, link))
	{
	  switch (proto->proto)
	    {
	    case IPSEC_PROTO_IPSEC_AH:
	      ah_present = "yes";
	      switch (proto->id)
		{
		case IPSEC_AH_MD5:
		  ah_hash_alg = "md5";
		  break;

		case IPSEC_AH_SHA:
		  ah_hash_alg = "sha";
		  break;

		case IPSEC_AH_DES:
		  ah_hash_alg = "des";
		  break;
		}

	      break;

	    case IPSEC_PROTO_IPSEC_ESP:
	      esp_present = "yes";
	      switch (proto->id)
		{
		case IPSEC_ESP_DES_IV64:
		  esp_enc_alg = "des-iv64";
		  break;

		case IPSEC_ESP_DES:
		  esp_enc_alg = "des";
		  break;

		case IPSEC_ESP_3DES:
		  esp_enc_alg = "3des";
		  break;

		case IPSEC_ESP_RC5:
		  esp_enc_alg = "rc5";
		  break;

		case IPSEC_ESP_IDEA:
		  esp_enc_alg = "idea";
		  break;

		case IPSEC_ESP_CAST:
		  esp_enc_alg = "cast";
		  break;

		case IPSEC_ESP_BLOWFISH:
		  esp_enc_alg = "blowfish";
		  break;

		case IPSEC_ESP_3IDEA:
		  esp_enc_alg = "3idea";
		  break;

		case IPSEC_ESP_DES_IV32:
		  esp_enc_alg = "des-iv32";
		  break;

		case IPSEC_ESP_RC4:
		  esp_enc_alg = "rc4";
		  break;

		case IPSEC_ESP_NULL:
		  esp_enc_alg = "null";
		  break;
		}

	      break;

	    case IPSEC_PROTO_IPCOMP:
	      comp_present = "yes";
	      switch (proto->id)
		{
		case IPSEC_IPCOMP_OUI:
		  comp_alg = "oui";
		  break;

		case IPSEC_IPCOMP_DEFLATE:
		  comp_alg = "deflate";
		  break;

		case IPSEC_IPCOMP_LZS:
		  comp_alg = "lzs";
		  break;

		case IPSEC_IPCOMP_V42BIS:
		  comp_alg = "v42bis";
		  break;
		}

	      break;
	    }

	  for (attr = proto->chosen->p + ISAKMP_TRANSFORM_SA_ATTRS_OFF;
	       attr
		 < proto->chosen->p + GET_ISAKMP_GEN_LENGTH (proto->chosen->p);
	       attr = value + len)
	    {
	      if (attr + ISAKMP_ATTR_VALUE_OFF
		  > (proto->chosen->p
		     + GET_ISAKMP_GEN_LENGTH (proto->chosen->p)))
		return "";
		
	      type = GET_ISAKMP_ATTR_TYPE (attr);
	      fmt = ISAKMP_ATTR_FORMAT (type);
	      type = ISAKMP_ATTR_TYPE (type);
	      value = attr + (fmt ? ISAKMP_ATTR_LENGTH_VALUE_OFF :
			      ISAKMP_ATTR_VALUE_OFF);
	      len = (fmt ? ISAKMP_ATTR_LENGTH_VALUE_LEN :
		     GET_ISAKMP_ATTR_LENGTH_VALUE (attr));

	      if (value + len > proto->chosen->p +
		  GET_ISAKMP_GEN_LENGTH (proto->chosen->p))
		return "";

	      switch (type)
		{
		case IPSEC_ATTR_SA_LIFE_TYPE:
		  lifetype = decode_16 (value);
		  break;

		case IPSEC_ATTR_SA_LIFE_DURATION:
		  switch (proto->proto)
		    {
		    case IPSEC_PROTO_IPSEC_AH:
		      if (lifetype == IPSEC_DURATION_SECONDS)
			{
			  if (len == 2)
			    sprintf (ah_life_seconds, "%d",
				     decode_16 (value));
			  else
			    sprintf (ah_life_seconds, "%d",
				     decode_32 (value));
			}
		      else
			{
			  if (len == 2)
			    sprintf (ah_life_kbytes, "%d",
				     decode_16 (value));
			  else
			    sprintf (ah_life_kbytes, "%d",
				     decode_32 (value));
			}

		      break;

		    case IPSEC_PROTO_IPSEC_ESP:
		      if (lifetype == IPSEC_DURATION_SECONDS)
			{
			  if (len == 2)
			    sprintf (esp_life_seconds, "%d",
				     decode_16 (value));
			  else
			    sprintf (esp_life_seconds, "%d",
				     decode_32 (value));
			}
		      else
			{
			  if (len == 2)
			    sprintf (esp_life_kbytes, "%d",
				     decode_16 (value));
			  else
			    sprintf (esp_life_kbytes, "%d",
				     decode_32 (value));
			}

		      break;

		    case IPSEC_PROTO_IPCOMP:
		      if (lifetype == IPSEC_DURATION_SECONDS)
			{
			  if (len == 2)
			    sprintf (comp_life_seconds, "%d",
				     decode_16 (value));
			  else
			    sprintf (comp_life_seconds, "%d",
				     decode_32 (value));
			}
		      else
			{
			  if (len == 2)
			    sprintf (comp_life_kbytes, "%d",
				     decode_16 (value));
			  else
			    sprintf (comp_life_kbytes, "%d",
				     decode_32 (value));
			}

		      break;
		    }
		  break;

		case IPSEC_ATTR_GROUP_DESCRIPTION:
		  switch (proto->proto)
		    {
		    case IPSEC_PROTO_IPSEC_AH:
		      sprintf (ah_group_desc, "%d", decode_16 (value));
		      break;

		    case IPSEC_PROTO_IPSEC_ESP:
		      sprintf (esp_group_desc, "%d",
			       decode_16 (value));
		      break;

		    case IPSEC_PROTO_IPCOMP:
		      sprintf (comp_group_desc, "%d",
			       decode_16 (value));
		      break;
		    }
		  break;

		case IPSEC_ATTR_ENCAPSULATION_MODE:
		  if (decode_16 (value) == IPSEC_ENCAP_TUNNEL)
		    switch (proto->proto)
		      {
		      case IPSEC_PROTO_IPSEC_AH:
			ah_encapsulation = "tunnel";
			break;

		      case IPSEC_PROTO_IPSEC_ESP:
			esp_encapsulation = "tunnel";
			break;

		      case IPSEC_PROTO_IPCOMP:
			comp_encapsulation = "tunnel";
			break;
		      }
		  else
		    switch (proto->proto)
		      {
		      case IPSEC_PROTO_IPSEC_AH:
			ah_encapsulation = "transport";
			break;

		      case IPSEC_PROTO_IPSEC_ESP:
			esp_encapsulation = "transport";
			break;

		      case IPSEC_PROTO_IPCOMP:
			comp_encapsulation = "transport";
			break;
		      }
		  break;

		case IPSEC_ATTR_AUTHENTICATION_ALGORITHM:
		  switch (proto->proto)
		    {
		    case IPSEC_PROTO_IPSEC_AH:
		      switch (decode_16 (value))
			{
			case IPSEC_AUTH_HMAC_MD5:
			  ah_auth_alg = "hmac-md5";
			  break;

			case IPSEC_AUTH_HMAC_SHA:
			  ah_auth_alg = "hmac-sha";
			  break;

			case IPSEC_AUTH_DES_MAC:
			  ah_auth_alg = "des-mac";
			  break;

			case IPSEC_AUTH_KPDK:
			  ah_auth_alg = "kpdk";
			  break;
			}
		      break;

		    case IPSEC_PROTO_IPSEC_ESP:
		      switch (decode_16 (value))
			{
			case IPSEC_AUTH_HMAC_MD5:
			  esp_auth_alg = "hmac-md5";
			  break;

			case IPSEC_AUTH_HMAC_SHA:
			  esp_auth_alg = "hmac-sha";
			  break;

			case IPSEC_AUTH_DES_MAC:
			  esp_auth_alg = "des-mac";
			  break;

			case IPSEC_AUTH_KPDK:
			  esp_auth_alg = "kpdk";
			  break;
			}
		      break;
		    }
		  break;

		case IPSEC_ATTR_KEY_LENGTH:
		  switch (proto->proto)
		    {
		    case IPSEC_PROTO_IPSEC_AH:
		      sprintf (ah_key_length, "%d", decode_16 (value));
		      break;

		    case IPSEC_PROTO_IPSEC_ESP:
		      sprintf (esp_key_length, "%d",
			       decode_16 (value));
		      break;
		    }
		  break;

		case IPSEC_ATTR_KEY_ROUNDS:
		  switch (proto->proto)
		    {
		    case IPSEC_PROTO_IPSEC_AH:
		      sprintf (ah_key_rounds, "%d", decode_16 (value));
		      break;

		    case IPSEC_PROTO_IPSEC_ESP:
		      sprintf (esp_key_rounds, "%d",
			       decode_16 (value));
		      break;
		    }
		  break;

		case IPSEC_ATTR_COMPRESS_DICTIONARY_SIZE:
		  sprintf (comp_dict_size, "%d", decode_16 (value));
		  break;

		case IPSEC_ATTR_COMPRESS_PRIVATE_ALGORITHM:
		  sprintf (comp_private_alg, "%d", decode_16 (value));
		  break;
		}
	    }
	}

      /* XXX IPv4-specific.  */
      policy_sa->transport->vtbl->get_src (policy_sa->transport,
					   (struct sockaddr **) &sin, &fmt);
      my_inet_ntop4 (&(sin->sin_addr.s_addr), local_ike_address,
		     sizeof local_ike_address - 1, 0);

      policy_sa->transport->vtbl->get_dst (policy_sa->transport,
					   (struct sockaddr **) &sin, &fmt);
      my_inet_ntop4 (&(sin->sin_addr.s_addr), remote_ike_address,
		     sizeof remote_ike_address - 1, 0);

      if (policy_isakmp_sa->initiator)
        {
	  id = policy_isakmp_sa->id_r;
	  id_sz = policy_isakmp_sa->id_r_len;
	}
      else
        {
	  id = policy_isakmp_sa->id_i;
	  id_sz = policy_isakmp_sa->id_i_len;
	}

      switch (id[0])
        {
	case IPSEC_ID_IPV4_ADDR:
	  remote_id_type = "IPv4 address";

	  net = decode_32 (id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ);
	  my_inet_ntop4 (&net, remote_id_addr_upper,
			 sizeof remote_id_addr_upper - 1, 1);
	  my_inet_ntop4 (&net, remote_id_addr_lower,
			 sizeof remote_id_addr_lower - 1, 1);
	  remote_id = strdup (remote_id_addr_upper);
	  if (!remote_id)
	    log_fatal ("policy_callback: strdup (\"%s\") failed",
		       remote_id_addr_upper);
	  break;

	case IPSEC_ID_IPV4_RANGE:
	  remote_id_type = "IPv4 range";

	  net = decode_32 (id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ);
	  my_inet_ntop4 (&net, remote_id_addr_lower,
			 sizeof remote_id_addr_lower - 1, 1);
	  net = decode_32 (id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ + 4);
	  my_inet_ntop4 (&net, remote_id_addr_upper,
			 sizeof remote_id_addr_upper - 1, 1);
	  remote_id = calloc (strlen (remote_id_addr_upper)
			      + strlen (remote_id_addr_lower) + 2,
			      sizeof (char));
	  if (!remote_id)
	    log_fatal ("policy_callback: calloc (%d, %d) failed",
		       strlen (remote_id_addr_upper)
		       + strlen (remote_id_addr_lower) + 2,
		       sizeof (char));

	  strcpy (remote_id, remote_id_addr_lower);
	  remote_id[strlen (remote_id_addr_lower)] = '-';
	  strcpy (remote_id + strlen (remote_id_addr_lower) + 1,
		  remote_id_addr_upper);
	  break;

	case IPSEC_ID_IPV4_ADDR_SUBNET:
	  remote_id_type = "IPv4 subnet";

	  net = decode_32 (id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ);
	  subnet = decode_32 (id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ + 4);
	  net &= subnet;
	  my_inet_ntop4 (&net, remote_id_addr_lower,
			 sizeof remote_id_addr_lower - 1, 1);
	  net |= ~subnet;
	  my_inet_ntop4 (&net, remote_id_addr_upper,
			 sizeof remote_id_addr_upper - 1, 1);
	  remote_id = calloc (strlen (remote_id_addr_upper)
			      + strlen (remote_id_addr_lower) + 2,
			      sizeof (char));
	  if (!remote_id)
	    log_fatal ("policy_callback: calloc (%d, %d) failed",
		       strlen (remote_id_addr_upper)
		       + strlen (remote_id_addr_lower) + 2,
		       sizeof (char));

	  strcpy (remote_id, remote_id_addr_lower);
	  remote_id[strlen (remote_id_addr_lower)] = '-';
	  strcpy (remote_id + strlen (remote_id_addr_lower) + 1,
		  remote_id_addr_upper);
	  break;

	case IPSEC_ID_IPV6_ADDR: /* XXX we need decode_128 ().  */
	  remote_id_type = "IPv6 address";
	  break;

	case IPSEC_ID_IPV6_RANGE: /* XXX we need decode_128 ().  */
	  remote_id_type = "IPv6 range";
	  break;

	case IPSEC_ID_IPV6_ADDR_SUBNET: /* XXX we need decode_128 ().  */
	  remote_id_type = "IPv6 address";
	  break;

	case IPSEC_ID_FQDN:
	  remote_id_type = "FQDN";
	  remote_id = calloc (id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ + 1,
			      sizeof (char));
	  if (!remote_id)
	    log_fatal ("policy_callback: calloc (%d, %d) failed",
		       id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ + 1,
		       sizeof (char));
	  memcpy (remote_id, id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ, 
		  id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ);
	  break;

	case IPSEC_ID_USER_FQDN:
	  remote_id_type = "User FQDN";
	  remote_id = calloc (id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ + 1,
			      sizeof (char));
	  if (!remote_id)
	    log_fatal ("policy_callback: calloc (%d, %d) failed",
		       id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ + 1,
		       sizeof (char));
	  memcpy (remote_id, id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ, 
		  id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ);
	  break;

	case IPSEC_ID_DER_ASN1_DN: /* XXX -- not sure what's in this.  */
	  remote_id_type = "ASN1 DN";
	  break;

	case IPSEC_ID_DER_ASN1_GN: /* XXX -- not sure what's in this.  */
	  remote_id_type = "ASN1 GN";
	  break;

	case IPSEC_ID_KEY_ID: /* XXX -- hex-encode this.  */
	  remote_id_type = "Key ID";
	  break;

	default:
	  log_print ("policy_callback: unknown remote ID type %d", id[0]);
	  return "";
	}

      switch (id[1])
        {
	case IPPROTO_TCP:
	  remote_id_proto = "tcp";
	  break;

	case IPPROTO_UDP:
	  remote_id_proto = "udp";
	  break;
	}

      snprintf (remote_id_port, sizeof remote_id_port - 1, "%d",
		decode_16 (id + 2));

      /* Initialize the ID variables.  */
      if (ie->id_ci)
        {
	  switch (GET_ISAKMP_ID_TYPE (ie->id_ci))
	    {
	    case IPSEC_ID_IPV4_ADDR:
	      remote_filter_type = "IPv4 address";

	      net = decode_32 (ie->id_ci + ISAKMP_ID_DATA_OFF);
	      my_inet_ntop4 (&net, remote_filter_addr_upper,
			     sizeof remote_filter_addr_upper - 1, 1);
	      my_inet_ntop4 (&net, remote_filter_addr_lower,
			     sizeof (remote_filter_addr_lower) - 1, 1);
	      remote_filter = strdup (remote_filter_addr_upper);
	      if (!remote_filter)
		log_fatal ("policy_callback: strdup (\"%s\") failed",
			   remote_filter_addr_upper);
	      break;

	    case IPSEC_ID_IPV4_RANGE:
	      remote_filter_type = "IPv4 range";

	      net = decode_32 (ie->id_ci + ISAKMP_ID_DATA_OFF);
	      my_inet_ntop4 (&net, remote_filter_addr_lower,
			     sizeof remote_filter_addr_lower - 1, 1);
	      net = decode_32 (ie->id_ci + ISAKMP_ID_DATA_OFF + 4);
	      my_inet_ntop4 (&net, remote_filter_addr_upper,
			     sizeof remote_filter_addr_upper - 1, 1);
	      remote_filter = calloc (strlen (remote_filter_addr_upper)
				      + strlen (remote_filter_addr_lower) + 2,
				      sizeof (char));
	      if (!remote_filter)
		log_fatal ("policy_callback: calloc (%d, %d) failed",
			   strlen (remote_filter_addr_upper)
			   + strlen (remote_filter_addr_lower) + 2,
			   sizeof (char));
	      strcpy (remote_filter, remote_filter_addr_lower);
	      remote_filter[strlen (remote_filter_addr_lower)] = '-';
	      strcpy (remote_filter + strlen (remote_filter_addr_lower) + 1,
		      remote_filter_addr_upper);
	      break;

	    case IPSEC_ID_IPV4_ADDR_SUBNET:
	      remote_filter_type = "IPv4 subnet";

	      net = decode_32 (ie->id_ci + ISAKMP_ID_DATA_OFF);
	      subnet = decode_32 (ie->id_ci + ISAKMP_ID_DATA_OFF + 4);
	      net &= subnet;
	      my_inet_ntop4 (&net, remote_filter_addr_lower,
			     sizeof remote_filter_addr_lower - 1, 1);
	      net |= ~subnet;
	      my_inet_ntop4 (&net, remote_filter_addr_upper,
			     sizeof remote_filter_addr_upper - 1, 1);
	      remote_filter = calloc (strlen (remote_filter_addr_upper)
				      + strlen (remote_filter_addr_lower) + 2,
				      sizeof (char));
	      if (!remote_filter)
		log_fatal ("policy_callback: calloc (%d, %d) failed",
			   strlen (remote_filter_addr_upper)
			   + strlen (remote_filter_addr_lower) + 2,
			   sizeof (char));
	      strcpy (remote_filter, remote_filter_addr_lower);
	      remote_filter[strlen (remote_filter_addr_lower)] = '-';
	      strcpy (remote_filter + strlen (remote_filter_addr_lower) + 1,
		      remote_filter_addr_upper);
	      break;

	    case IPSEC_ID_IPV6_ADDR: /* XXX we need decode_128 (). */
	      remote_filter_type = "IPv6 address";
	      break;

	    case IPSEC_ID_IPV6_RANGE: /* XXX we need decode_128 ().  */
	      remote_filter_type = "IPv6 range";
	      break;

	    case IPSEC_ID_IPV6_ADDR_SUBNET: /* XXX we need decode_128 ().  */
	      remote_filter_type = "IPv6 address";
	      break;

	    case IPSEC_ID_FQDN:
	      remote_filter_type = "FQDN";
	      remote_filter = calloc (ie->id_ci_sz - ISAKMP_ID_DATA_OFF + 1,
				      sizeof (char));
	      if (!remote_filter)
		log_fatal ("policy_callback: calloc (%d, %d) failed",
			   ie->id_ci_sz - ISAKMP_ID_DATA_OFF + 1,
			   sizeof (char));
	      memcpy (remote_filter, ie->id_ci + ISAKMP_ID_DATA_OFF,
		      ie->id_ci_sz);
	      break;

	    case IPSEC_ID_USER_FQDN:
	      remote_filter_type = "User FQDN";
	      remote_filter = calloc (ie->id_ci_sz - ISAKMP_ID_DATA_OFF + 1,
				      sizeof (char));
	      if (!remote_filter)
		log_fatal ("policy_callback: calloc (%d, %d) failed",
			   ie->id_ci_sz - ISAKMP_ID_DATA_OFF + 1,
			   sizeof (char));
	      memcpy (remote_filter, ie->id_ci + ISAKMP_ID_DATA_OFF,
		      ie->id_ci_sz);
	      break;

	    case IPSEC_ID_DER_ASN1_DN: /* XXX -- not sure what's in this.  */
	      remote_filter_type = "ASN1 DN";
	      break;

	    case IPSEC_ID_DER_ASN1_GN: /* XXX -- not sure what's in this.  */
	      remote_filter_type = "ASN1 GN";
	      break;

	    case IPSEC_ID_KEY_ID: /* XXX -- hex-encode this.  */
	      remote_filter_type = "Key ID";
	      break;

	    default:
	      log_print ("policy_callback: unknown initiator ID type %d",
			 GET_ISAKMP_ID_TYPE (ie->id_ci));
	      return "";
	    }

	  switch (ie->id_ci[ISAKMP_GEN_SZ + 1])
	    {
	    case IPPROTO_TCP:
	      remote_filter_proto = "tcp";
	      break;

	    case IPPROTO_UDP:
	      remote_filter_proto = "udp";
	      break;
	    }

	  snprintf (remote_filter_port, sizeof remote_filter_port - 1,
		    "%d", decode_16 (ie->id_ci + ISAKMP_GEN_SZ + 2));
	}
      else
        {
	  policy_sa->transport->vtbl->get_src (policy_sa->transport,
					       (struct sockaddr **) &sin,
					       &fmt);
	  remote_filter_type = "IPv4 address";

	  my_inet_ntop4 (&(sin->sin_addr.s_addr), remote_filter_addr_upper,
			 sizeof remote_filter_addr_upper - 1, 0);
	  my_inet_ntop4 (&(sin->sin_addr.s_addr), remote_filter_addr_lower,
			 sizeof remote_filter_addr_lower - 1, 1);
	  remote_filter = strdup (remote_filter_addr_upper);
	  if (!remote_filter)
	    log_fatal ("policy_callback: strdup (\"%s\") failed",
		       remote_filter_addr_upper);
	}

      if (ie->id_cr)
        {
	  switch (GET_ISAKMP_ID_TYPE (ie->id_cr))
	    {
	    case IPSEC_ID_IPV4_ADDR:
	      local_filter_type = "IPv4 address";

	      net = decode_32 (ie->id_cr + ISAKMP_ID_DATA_OFF);
	      my_inet_ntop4 (&net, local_filter_addr_upper,
			     sizeof local_filter_addr_upper - 1, 1);
	      my_inet_ntop4 (&net, local_filter_addr_lower,
			     sizeof local_filter_addr_upper - 1, 1);
	      local_filter = strdup (local_filter_addr_upper);
	      if (!local_filter)
		log_fatal ("policy_callback: strdup (\"%s\") failed",
			   local_filter_addr_upper);
	      break;

	    case IPSEC_ID_IPV4_RANGE:
	      local_filter_type = "IPv4 range";

	      net = decode_32 (ie->id_cr + ISAKMP_ID_DATA_OFF);
	      my_inet_ntop4 (&net, local_filter_addr_lower,
			     sizeof local_filter_addr_lower - 1, 1);
	      net = decode_32 (ie->id_cr + ISAKMP_ID_DATA_OFF + 4);
	      my_inet_ntop4 (&net, local_filter_addr_upper,
			     sizeof local_filter_addr_upper - 1, 1);
	      local_filter = calloc (strlen (local_filter_addr_upper)
				     + strlen (local_filter_addr_lower) + 2,
				     sizeof (char));
	      if (!local_filter)
		log_fatal ("policy_callback: calloc (%d, %d) failed",
			   strlen (local_filter_addr_upper)
			   + strlen (local_filter_addr_lower) + 2,
			   sizeof (char));
	      strcpy (local_filter, local_filter_addr_lower);
	      local_filter[strlen (local_filter_addr_lower)] = '-';
	      strcpy (local_filter + strlen (local_filter_addr_lower) + 1,
		      local_filter_addr_upper);
	      break;

	    case IPSEC_ID_IPV4_ADDR_SUBNET:
	      local_filter_type = "IPv4 subnet";

	      net = decode_32 (ie->id_cr + ISAKMP_ID_DATA_OFF);
	      subnet = decode_32 (ie->id_cr + ISAKMP_ID_DATA_OFF + 4);
	      net &= subnet;
	      my_inet_ntop4 (&net, local_filter_addr_lower,
			     sizeof local_filter_addr_lower - 1, 1);
	      net |= ~subnet;
	      my_inet_ntop4 (&net, local_filter_addr_upper,
			     sizeof local_filter_addr_upper - 1, 1);
	      local_filter = calloc (strlen (local_filter_addr_upper)
				     + strlen (local_filter_addr_lower) + 2,
				     sizeof (char));
	      if (!local_filter)
		log_fatal ("policy_callback: calloc (%d, %d) failed",
			   strlen (local_filter_addr_upper)
			   + strlen (local_filter_addr_lower) + 2,
			   sizeof (char));
	      strcpy (local_filter, local_filter_addr_lower);
	      local_filter[strlen (local_filter_addr_lower)] = '-';
	      strcpy (local_filter + strlen (local_filter_addr_lower) + 1,
		      local_filter_addr_upper);
	      break;

	    case IPSEC_ID_IPV6_ADDR: /* XXX we need decode_128 ().  */
	      local_filter_type = "IPv6 address";
	      break;

	    case IPSEC_ID_IPV6_RANGE: /* XXX we need decode_128 ().  */
	      local_filter_type = "IPv6 range";
	      break;

	    case IPSEC_ID_IPV6_ADDR_SUBNET: /* XXX we need decode_128 ().  */
	      local_filter_type = "IPv6 address";
	      break;

	    case IPSEC_ID_FQDN:
	      local_filter_type = "FQDN";
	      local_filter = calloc (ie->id_cr_sz - ISAKMP_ID_DATA_OFF + 1,
				     sizeof (char));
	      if (!local_filter)
		log_fatal ("policy_callback: calloc (%d, %d) failed",
			   ie->id_cr_sz - ISAKMP_ID_DATA_OFF + 1,
			   sizeof (char));
	      memcpy (local_filter, ie->id_cr + ISAKMP_ID_DATA_OFF,
		      ie->id_cr_sz);
	      break;

	    case IPSEC_ID_USER_FQDN:
	      local_filter_type = "User FQDN";
	      local_filter = calloc (ie->id_cr_sz - ISAKMP_ID_DATA_OFF + 1,
				     sizeof (char));
	      if (!local_filter)
		log_fatal ("policy_callback: calloc (%d, %d) failed",
			   ie->id_cr_sz - ISAKMP_ID_DATA_OFF + 1,
			   sizeof (char));
	      memcpy (local_filter, ie->id_cr + ISAKMP_ID_DATA_OFF,
		      ie->id_cr_sz);
	      break;

	    case IPSEC_ID_DER_ASN1_DN: /* XXX -- not sure what's in this.  */
	      local_filter_type = "ASN1 DN";
	      break;

	    case IPSEC_ID_DER_ASN1_GN: /* XXX -- not sure what's in this.  */
	      local_filter_type = "ASN1 GN";
	      break;

	    case IPSEC_ID_KEY_ID: /* XXX -- hex-encode this.  */
	      local_filter_type = "Key ID";
	      break;

	    default:
	      log_print ("policy_callback: unknown responder ID type %d",
			 GET_ISAKMP_ID_TYPE (ie->id_cr));
	      return "";
	    }

	  switch (ie->id_cr[ISAKMP_GEN_SZ + 1])
	    {
	    case IPPROTO_TCP:
	      local_filter_proto = "tcp";
	      break;

	    case IPPROTO_UDP:
	      local_filter_proto = "udp";
	      break;
	    }

	  snprintf (local_filter_port, sizeof local_filter_port - 1,
		    "%d", decode_16 (ie->id_cr + ISAKMP_GEN_SZ + 2));
	}
      else
        {
	  policy_sa->transport->vtbl->get_dst (policy_sa->transport,
					       (struct sockaddr **) &sin,
					       &fmt);
	  local_filter_type = "IPv4 address";

	  my_inet_ntop4 (&(sin->sin_addr.s_addr), local_filter_addr_upper,
			 sizeof local_filter_addr_upper - 1, 0);
	  my_inet_ntop4 (&(sin->sin_addr.s_addr), local_filter_addr_lower,
			 sizeof local_filter_addr_lower - 1, 1);
	  local_filter = strdup (local_filter_addr_upper);
	  if (!local_filter)
		log_fatal ("policy_callback: strdup (\"%s\") failed",
			   local_filter_addr_upper);
        }

#if 0
      printf ("esp_present == %s\n", esp_present);
      printf ("ah_present == %s\n", ah_present);
      printf ("comp_present == %s\n", comp_present);
      printf ("ah_hash_alg == %s\n", ah_hash_alg);
      printf ("esp_enc_alg == %s\n", esp_enc_alg);
      printf ("comp_alg == %s\n", comp_alg);
      printf ("ah_auth_alg == %s\n", ah_auth_alg);
      printf ("esp_auth_alg == %s\n", esp_auth_alg);
      printf ("ah_life_seconds == %s\n", ah_life_seconds);
      printf ("ah_life_kbytes == %s\n", ah_life_kbytes);
      printf ("esp_life_seconds == %s\n", esp_life_seconds);
      printf ("esp_life_kbytes == %s\n", esp_life_kbytes);
      printf ("comp_life_seconds == %s\n", comp_life_seconds);
      printf ("comp_life_kbytes == %s\n", comp_life_kbytes);
      printf ("ah_encapsulation == %s\n", ah_encapsulation);
      printf ("esp_encapsulation == %s\n", esp_encapsulation);
      printf ("comp_encapsulation == %s\n", comp_encapsulation);
      printf ("comp_dict_size == %s\n", comp_dict_size);
      printf ("comp_private_alg == %s\n", comp_private_alg);
      printf ("ah_key_length == %s\n", ah_key_length);
      printf ("ah_key_rounds == %s\n", ah_key_rounds);
      printf ("esp_key_length == %s\n", esp_key_length);
      printf ("esp_key_rounds == %s\n", esp_key_rounds);
      printf ("ah_group_desc == %s\n", ah_group_desc);
      printf ("esp_group_desc == %s\n", esp_group_desc);
      printf ("comp_group_desc == %s\n", comp_group_desc);
      printf ("remote_filter_type == %s\n", remote_filter_type);
      printf ("remote_filter_addr_upper == %s\n", remote_filter_addr_upper);
      printf ("remote_filter_addr_lower == %s\n", remote_filter_addr_lower);
      printf ("remote_filter == %s\n", remote_filter);
      printf ("remote_filter_port == %s\n", remote_filter_port);
      printf ("remote_filter_proto == %s\n", remote_filter_proto);
      printf ("local_filter_type == %s\n", local_filter_type);
      printf ("local_filter_addr_upper == %s\n", local_filter_addr_upper);
      printf ("local_filter_addr_lower == %s\n", local_filter_addr_lower);
      printf ("local_filter == %s\n", local_filter);
      printf ("local_filter_port == %s\n", local_filter_port);
      printf ("local_filter_proto == %s\n", local_filter_proto);
      printf ("remote_id_type == %s\n", remote_id_type);
      printf ("remote_id_addr_upper == %s\n", remote_id_addr_upper);
      printf ("remote_id_addr_lower == %s\n", remote_id_addr_lower);
      printf ("remote_id == %s\n", remote_id);
      printf ("remote_id_port == %s\n", remote_id_port);
      printf ("remote_id_proto == %s\n", remote_id_proto);
      printf ("remote_ike_address == %s\n", remote_ike_address);
      printf ("local_ike_address == %s\n", local_ike_address);
      printf ("pfs == %s\n", pfs);
#endif /* 0 */

      /* Unset dirty now.  */
      dirty = 0;
    }

  if (strcmp (name, "GMTTimeOfDay") == 0)
    {
	tt = time((time_t) NULL);
	strftime (mytimeofday, 14, "%G%m%d%H%M%S", gmtime(&tt));
	return mytimeofday;
    }

  if (strcmp (name, "LocalTimeOfDay") == 0)
    {
	tt = time((time_t) NULL);
	strftime (mytimeofday, 14, "%G%m%d%H%M%S", localtime(&tt));
	return mytimeofday;
    }

  if (strcmp (name, "pfs") == 0)
    return pfs;

  if (strcmp (name, "app_domain") == 0)
    return "IPsec policy";

  if (strcmp (name, "doi") == 0)
    return "ipsec";

  if (strcmp (name, "esp_present") == 0)
    return esp_present;

  if (strcmp (name, "ah_present") == 0)
    return ah_present;

  if (strcmp (name, "comp_present") == 0)
    return comp_present;

  if (strcmp (name, "ah_hash_alg") == 0)
    return ah_hash_alg;

  if (strcmp (name, "ah_auth_alg") == 0)
    return ah_auth_alg;

  if (strcmp (name, "esp_auth_alg") == 0)
    return esp_auth_alg;

  if (strcmp (name, "esp_enc_alg") == 0)
    return esp_enc_alg;

  if (strcmp (name, "comp_alg") == 0)
    return comp_alg;

  if (strcmp (name, "ah_life_kbytes") == 0)
    return ah_life_kbytes;

  if (strcmp (name, "ah_life_seconds") == 0)
    return ah_life_seconds;

  if (strcmp (name, "esp_life_kbytes") == 0)
    return ah_life_kbytes;

  if (strcmp (name, "esp_life_seconds") == 0)
    return ah_life_seconds;

  if (strcmp (name, "comp_life_kbytes") == 0)
    return comp_life_kbytes;

  if (strcmp (name, "comp_life_seconds") == 0)
    return comp_life_seconds;

  if (strcmp (name, "ah_encapsulation") == 0)
    return ah_encapsulation;

  if (strcmp (name, "esp_encapsulation") == 0)
    return esp_encapsulation;

  if (strcmp (name, "comp_encapsulation") == 0)
    return comp_encapsulation;

  if (strcmp (name, "ah_key_length") == 0)
    return ah_key_length;

  if (strcmp (name, "ah_key_rounds") == 0)
    return ah_key_rounds;

  if (strcmp (name, "esp_key_length") == 0)
    return esp_key_length;

  if (strcmp (name, "esp_key_rounds") == 0)
    return esp_key_rounds;

  if (strcmp (name, "comp_dict_size") == 0)
    return comp_dict_size;

  if (strcmp (name, "comp_private_alg") == 0)
    return comp_private_alg;

  if (strcmp (name, "remote_filter_type") == 0)
    return remote_filter_type;

  if (strcmp (name, "remote_filter") == 0)
    return remote_filter;

  if (strcmp (name, "remote_filter_addr_upper") == 0)
    return remote_filter_addr_upper;

  if (strcmp (name, "remote_filter_addr_lower") == 0)
    return remote_filter_addr_lower;

  if (strcmp (name, "remote_filter_port") == 0)
    return remote_filter_port;

  if (strcmp (name, "remote_filter_proto") == 0)
    return remote_filter_proto;

  if (strcmp (name, "local_filter_type") == 0)
    return local_filter_type;

  if (strcmp (name, "local_filter") == 0)
    return local_filter;

  if (strcmp (name, "local_filter_addr_upper") == 0)
    return local_filter_addr_upper;

  if (strcmp (name, "local_filter_addr_lower") == 0)
    return local_filter_addr_lower;

  if (strcmp (name, "local_filter_port") == 0)
    return local_filter_port;

  if (strcmp (name, "local_filter_proto") == 0)
    return local_filter_proto;

  if (strcmp (name, "remote_ike_address") == 0)
    return remote_ike_address;

  if (strcmp (name, "local_ike_address") == 0)
    return local_ike_address;

  if (strcmp (name, "remote_id_type") == 0)
    return remote_id_type;

  if (strcmp (name, "remote_id") == 0)
    return remote_id;

  if (strcmp (name, "remote_id_addr_upper") == 0)
    return remote_id_addr_upper;

  if (strcmp (name, "remote_id_addr_lower") == 0)
    return remote_id_addr_lower;

  if (strcmp (name, "remote_id_port") == 0)
    return remote_id_port;

  if (strcmp (name, "remote_id_proto") == 0)
    return remote_id_proto;

  return "";
}

void
policy_init (void)
{
  char *ptr, *policy_file;
  char **asserts;
  struct stat st;
  int fd, len, i;

  log_debug (LOG_MISC, 50, "policy_init: initializing");

#if defined (HAVE_DLOPEN) && !defined (USE_KEYNOTE)
  if (!dyn_load (libkeynote_script))
    return;
#endif

  /* If there exists a session already, release all its resources.  */
  if (keynote_sessid != -1)
    LK (kn_close, (keynote_sessid));

  /* Initialize a session.  */
  keynote_sessid = LK (kn_init, ());
  if (keynote_sessid == -1)
    log_fatal ("policy_init: kn_init () failed");

  /* Get policy file from configuration.  */
  policy_file = conf_get_str ("General", "policy-file");
  if (!policy_file)
    policy_file = POLICY_FILE_DEFAULT;

  /* Open policy file.  */
  fd = open (policy_file, O_RDONLY);
  if (fd == -1)
    log_fatal ("policy_init: open (\"%s\", O_RDONLY) failed", policy_file);

  /* Get size.  */
  if (fstat (fd, &st) == -1)
    log_fatal ("policy_init: fstat (%d, &st) failed", fd);

  /* Allocate memory to keep policies.  */
  ptr = calloc (st.st_size + 1, sizeof (char));
  if (!ptr)
    log_fatal ("policy_init: calloc (%d, %d) failed", st.st_size,
	       sizeof (char));

  /* Just in case there are short reads... */
  for (len = 0; len < st.st_size; len += i)
    {
      i = read (fd, ptr + len, st.st_size - len);
      if (i == -1)
	log_fatal ("policy_init: read (%d, %p, %d) failed", fd, ptr + len,
		   st.st_size - len);
    }

  /* We're done with this.  */
  close (fd);

  /* Parse buffer, break up into individual policies.  */
  asserts = LK (kn_read_asserts, (ptr, st.st_size, &i));

  /* Begone!  */
  free (ptr);

  /* Add each individual policy in the session.  */
  for (fd = 0; fd < i; fd++)
    {
      if (LK (kn_add_assertion, (keynote_sessid, asserts[fd],
				 strlen (asserts[fd]), ASSERT_FLAG_LOCAL))
	  == -1)
        log_print ("policy_init: "
		   "kn_add_assertion (%d, %p, %d, ASSERT_FLAG_LOCAL) failed",
                   keynote_sessid, asserts[fd], strlen (asserts[fd]));

      free (asserts[fd]);
    }

  if (asserts)
    free (asserts);

  /* Add the callback that will handle attributes.  */
  if (LK (kn_add_action, (keynote_sessid, ".*", (char *) policy_callback,
			  ENVIRONMENT_FLAG_FUNC | ENVIRONMENT_FLAG_REGEX))
      == -1)
    log_fatal ("policy_init: "
	       "kn_add_action (%d, \".*\", %p, FUNC | REGEX) failed",
	       keynote_sessid, policy_callback);
}
