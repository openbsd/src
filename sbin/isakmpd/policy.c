/*	$OpenBSD: policy.c,v 1.1 1999/07/07 22:10:28 niklas Exp $	*/
/*	$EOM: policy.c,v 1.2 1999/06/07 08:46:34 niklas Exp $ */

/*
 * Copyright (c) 1999 Angelos D. Keromytis.  All rights reserved.
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

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <keynote.h>

#include "sysdep.h"

#include "app.h"
#include "conf.h"
#include "connection.h"
#include "cookie.h"
#include "doi.h"
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
#define POLICY_FILE_DEFAULT "/etc/isakmpd.policy"
#endif /* POLICY_FILE_DEFAULT */

int keynote_sessid = -1;

struct exchange *policy_exchange = NULL;
struct sa *policy_sa = NULL;

static char *
policy_callback (char *name)
{
  struct proto *proto;

  u_int8_t *attr, *value;
  u_int16_t len, type;
  int fmt, lifetype = 0;

  /* We use all these as a cache */
  static char *esp_present, *ah_present, *comp_present;
  static char *ah_hash_alg, *ah_auth_alg, *esp_auth_alg, *esp_enc_alg;
  static char *comp_alg, ah_life_kbytes[32], ah_life_seconds[32];
  static char esp_life_kbytes[32], esp_life_seconds[32], comp_life_kbytes[32];
  static char comp_life_seconds[32], *ah_encapsulation, *esp_encapsulation;
  static char *comp_encapsulation, ah_key_length[32], esp_key_length[32];
  static char ah_key_rounds[32], esp_key_rounds[32], comp_dict_size[32];
  static char comp_private_alg[32], *id_initiator_type, *id_responder_type;
  static char id_initiator_addr_upper[32], id_initiator_addr_lower[32];
  static char id_responder_addr_upper[32], id_responder_addr_lower[32];
  static char id_initiator[100], id_responder[100];
  static char ah_group_desc[32], esp_group_desc[32], comp_group_desc[32];
  
  static int dirty = 1;

  /* We only need to set dirty at initialization time really */
  if (strcmp (name, KEYNOTE_CALLBACK_CLEANUP) == 0 ||
      strcmp (name, KEYNOTE_CALLBACK_INITIALIZE) == 0)
    {
      esp_present = ah_present = comp_present = "no";
      ah_hash_alg = ah_auth_alg = NULL;
      esp_auth_alg = esp_enc_alg = comp_alg = ah_encapsulation = NULL;
      esp_encapsulation = comp_encapsulation = id_initiator_type = NULL;
      id_responder_type = NULL;
      memset (ah_life_kbytes, 0, 32);
      memset (ah_life_seconds, 0, 32);
      memset (esp_life_kbytes, 0, 32);
      memset (esp_life_seconds, 0, 32);
      memset (comp_life_kbytes, 0, 32);
      memset (comp_life_seconds, 0, 32);
      memset (ah_key_length, 0, 32);
      memset (ah_key_rounds, 0, 32);
      memset (esp_key_length, 0, 32);
      memset (esp_key_rounds, 0, 32);
      memset (comp_dict_size, 0, 32);
      memset (comp_private_alg, 0, 32);
      memset (id_initiator_addr_upper, 0, 32);
      memset (id_initiator_addr_lower, 0, 32);
      memset (id_responder_addr_upper, 0, 32);
      memset (id_responder_addr_lower, 0, 32);
      memset (ah_group_desc, 0, 32);
      memset (esp_group_desc, 0, 32);
      memset (comp_group_desc, 0, 32);
      memset (id_initiator, 0, 100); /* XX */
      memset (id_responder, 0, 100); /* XX */
      
      dirty = 1;
      return "";
    }

  /* 
   * If dirty is set, this is the first request for an attribute, so
   * populate our value cache.
   */
  if (dirty)
  {
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
	       attr < proto->chosen->p + 
		      GET_ISAKMP_GEN_LENGTH (proto->chosen->p);
	       attr = value + len)
	  {
	      if (attr + ISAKMP_ATTR_VALUE_OFF > proto->chosen->p +
		  GET_ISAKMP_GEN_LENGTH (proto->chosen->p))
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
		    if (decode_16(value) == IPSEC_ENCAP_TUNNEL)
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
		    sprintf (comp_dict_size, "%d", decode_16(value));
		    break;

		  case IPSEC_ATTR_COMPRESS_PRIVATE_ALGORITHM:
		    sprintf (comp_private_alg, "%d", decode_16 (value));
		    break;
	      }
	  }
      }

      /* Unset dirty now */
      dirty = 0;
  }

  /* XXX Need to initialize the ID variables */

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

  if (strcmp (name, "id_initiator_type") == 0)
    return id_initiator_type;

  if (strcmp (name, "id_initiator") == 0)
    return id_initiator;

  if (strcmp (name, "id_initiator_addr_upper") == 0)
    return id_initiator_addr_upper;

  if (strcmp (name, "id_initiator_addr_lower") == 0)
    return id_initiator_addr_lower;

  if (strcmp (name, "id_responder_type") == 0)
    return id_responder_type;

  if (strcmp (name, "id_responder") == 0)
    return id_responder;

  if (strcmp (name, "id_responder_addr_upper") == 0)
    return id_responder_addr_upper;

  if (strcmp (name, "id_responder_addr_lower") == 0)
    return id_responder_addr_lower;

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

  /* If there exists a session already, release all its resources */
  if (keynote_sessid != -1)
    kn_close (keynote_sessid);

  /* Initialize a session */
  keynote_sessid = kn_init ();
  if (keynote_sessid == -1)
    log_fatal ("kn_init()");

  /* Get policy file from configuration */
  policy_file = conf_get_str ("General", "policy-file");
  if (!policy_file)
    policy_file = POLICY_FILE_DEFAULT;

  /* Open policy file */
  fd = open (policy_file, O_RDONLY);
  if (fd == -1)
    log_fatal ("open (\"%s\", O_RDONLY)", policy_file);

  /* Get size */
  if (fstat (fd, &st) == -1)
    log_fatal ("fstat (%d, &st)", fd);

  /* Allocate memory to keep policies */
  ptr = calloc (st.st_size + 1, sizeof (char));
  if (!ptr)
    log_fatal ("calloc (%d, %d)", st.st_size, sizeof (char));

  /* Just in case there's short reads... */
  for (len = 0; len < st.st_size; len += i)
    if ((i = read (fd, ptr + len, st.st_size - len)) == -1)
      log_fatal ("read (%d, %p, %d)", fd, ptr + len, st.st_size - len);

  /* We're done with this */
  close (fd);

  /* Parse buffer, break up into individual policies */
  asserts = kn_read_asserts (ptr, st.st_size, &i);

  /* Begone */
  free (ptr);

  /* Add each individual policy in the session */
  for (fd = 0; fd < i; fd++)
  {
      if (kn_add_assertion (keynote_sessid, asserts[fd], strlen (asserts[fd]),
                            ASSERT_FLAG_LOCAL) == -1)
        log_fatal ("kn_add_assertion (%d, %p, %d, ASSERT_FLAG_LOCAL)",
                   keynote_sessid, asserts[fd], strlen (asserts[fd]));

      free (asserts[fd]);
  }

  if (asserts)
    free (asserts);

  /* Add the callback that will handle attributes */
  if (kn_add_action (keynote_sessid, ".*", (char *) policy_callback,
		     ENVIRONMENT_FLAG_FUNC | ENVIRONMENT_FLAG_REGEX) == -1)
    log_fatal ("kn_add_action (%d, \".*\", %p, FUNC | REGEX)",
	       keynote_sessid, policy_callback);
}
