/*
 * Copyright 1997,1998 Niels Provos <provos@physnet.uni-hamburg.de>
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
 * The following functions handle the interaction of the Photuris daemon
 * with the PF_ENCAP interface as used by OpenBSD's IPSec implementation.
 * This is the only file which needs to be changed for making Photuris
 * work with other kernel interfaces.
 * The SPI object here can actually hold two SPIs, one for encryption
 * and one for authentication.
 */

#ifndef lint
static char rcsid[] = "$Id: kernel.c,v 1.11 1998/06/30 16:58:31 provos Exp $";
#endif

#include <time.h>
#include <sys/time.h>

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netns/ns.h>
#include <netiso/iso.h>
#include <netccitt/x25.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#define INET                     /* Needed for setting ipsec routes */
#include <net/encap.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ah.h>

#define _KERNEL_C_
#include "photuris.h"
#include "state.h"
#include "attributes.h"
#include "buffer.h"
#include "spi.h"
#include "kernel.h"
#include "errlog.h"
#include "server.h"
#ifdef DEBUG
#include "config.h"
#endif

#ifdef DEBUG
time_t now;

#define kernel_debug(x) {time(&now); printf("%.24s ", ctime(&now)); printf x;}
#else
#define kernel_debug(x)
#endif

static int sd;

/*
 * Translate a Photuris ID into a data structure for the 
 * corresponding Kernel transform.
 */

transform *
kernel_get_transform(int id)
{
     int i;

     for (i=sizeof(xf)/sizeof(transform)-1; i >= 0; i--) 
	  if (xf[i].photuris_id == id)
	       return &xf[i];
     return NULL;
}

int
kernel_known_transform(int id)
{
     return kernel_get_transform(id) == NULL ? -1 : 0;
}

/*
 * For ESP, we can specify an additional AH transform.
 * Not all combinations are possible.
 * Returns AT_ENC, when the ESP transform does not allow this AH.
 * Returns AT_AUTH, when the AH transform does not work with ESP.
 */

int
kernel_valid(attrib_t *enc, attrib_t *auth)
{
     transform *xf_enc, *xf_auth;

     xf_enc = kernel_get_transform(enc->id);
     xf_auth = kernel_get_transform(auth->id);

     if (xf_enc->flags & ESP_OLD) 
	  return AT_ENC;
     if (!(xf_auth->flags & ESP_NEW))
	  return AT_AUTH;
     return 0;
}

/*
 * Check if the chosen authentication transform, satisfies the
 * selected flags.
 */

int
kernel_valid_auth(attrib_t *auth, u_int8_t *flag, u_int16_t size)
{
     int i, hmac = 0;
     transform *xf_auth = kernel_get_transform(auth->id);

     if (xf_auth == NULL)
	  return -1; /* We don't know this attribute */

     for (i=0; i<size; i++) {
	  switch (flag[i]) {
	  case AT_HMAC:
	       hmac = 1;
	       break;
	  default:
	       break;
	  }
     }

     if (!hmac && !(xf_auth->flags & AH_OLD))
	  return -1;
     if (hmac && !(xf_auth->flags & AH_NEW))
	  return -1;

     return 0;
} 

int
init_kernel(void)
{
     if ((sd = socket(AF_ENCAP, SOCK_RAW, AF_UNSPEC)) < 0) 
	  crit_error(1, "socket() for IPSec in init_kernel()");
     return 1;
}

int
kernel_get_socket(void)
{
     return sd;
}

void
kernel_set_socket_policy(int sd)
{
     int level;

     /*
      * Need to bypass system security policy, so I can send and
      * receive key management datagrams in the clear.
      */

     level = IPSEC_LEVEL_BYPASS;   /* Did I mention I'm privileged? */
     if (setsockopt(sd, IPPROTO_IP, IP_AUTH_LEVEL, (char *)&level,
		    sizeof (int)) == -1)
	  crit_error(1, "setsockopt: can not bypass ipsec authentication policy");
     if (setsockopt(sd, IPPROTO_IP, IP_ESP_TRANS_LEVEL,
			(char *)&level, sizeof (int)) == -1)
	  crit_error(1, "setsockopt: can not bypass ipsec esp transport policy");
     if (setsockopt(sd, IPPROTO_IP, IP_ESP_NETWORK_LEVEL,
		    (char *)&level, sizeof (int)) == -1)
	  crit_error(1, "setsockopt: can not bypass ipsec esp network policy");
}

int
kernel_xf_set(struct encap_msghdr *em)
{
     if (write(sd, (char *)em, em->em_msglen) != em->em_msglen)
	  return 0;
     return 1;
}

int
kernel_xf_read(struct encap_msghdr *em, int msglen)
{
     if (read(sd, (char *)em, msglen) != msglen) {
	  log_error(1, "read() in kernel_xf_read()");
	  return 0;
     }
     return 1;
}

u_int32_t
kernel_reserve_spi(char *srcaddress, int options)
{
     u_int32_t spi;
     int proto;

     kernel_debug(("kernel_reserve_spi: %s\n", srcaddress));

     if ((options & (IPSEC_OPT_ENC|IPSEC_OPT_AUTH)) != 
	 (IPSEC_OPT_ENC|IPSEC_OPT_AUTH)) {
	  switch(options & (IPSEC_OPT_ENC|IPSEC_OPT_AUTH)) {
	  case IPSEC_OPT_ENC:
	       proto = IPPROTO_ESP;
	  default:
	       proto = IPPROTO_AH;
	  }
	  return kernel_reserve_single_spi(srcaddress, 0, proto);
     }

     if (!(spi = kernel_reserve_single_spi(srcaddress, 0, IPPROTO_ESP)))
	  return spi;
     
     /* Try to get the same spi for ah and esp */
     while (!kernel_reserve_single_spi(srcaddress, spi, IPPROTO_AH)) {
	  kernel_delete_spi(srcaddress, (u_int8_t *)&spi, IPPROTO_ESP);
	  if (!(spi = kernel_reserve_single_spi(srcaddress, 0, IPPROTO_ESP)))
	       return spi;
     }

     return spi;
}

u_int32_t
kernel_reserve_single_spi(char *srcaddress, u_int32_t spi, int proto)
{
     struct encap_msghdr *em;

     kernel_debug(("kernel_reserve_single_spi: %s, %08x\n", srcaddress, spi));

     bzero(buffer, EMT_RESERVESPI_FLEN);

     em = (struct encap_msghdr *)buffer;
     
     em->em_msglen = EMT_RESERVESPI_FLEN;
     em->em_version = PFENCAP_VERSION_1;
     em->em_type = EMT_RESERVESPI;

     em->em_gen_spi = spi;
     em->em_gen_dst.s_addr = inet_addr(srcaddress);
     em->em_gen_sproto = proto;
     
     if (!kernel_xf_set(em)) {
	  log_error(1, "kernel_xf_set() in kernel_reserve_single_spi()");
	  return 0;
     }

     if (!kernel_xf_read(em, EMT_RESERVESPI_FLEN))
	  return 0;

     return em->em_gen_spi;
}

int
kernel_ah(attrib_t *ob, struct spiob *SPI, u_int8_t *secrets, int hmac)
{
     struct encap_msghdr *em;
     struct ah_old_xencap *xdo;
     struct ah_new_xencap *xdn;
     transform *xf = kernel_get_transform(ob->id);

     if (xf == NULL || !(xf->flags & XF_AUTH)) {
	  log_error(0, "%d is not an auth transform in kernel_ah()", ob->id);
	  return -1;
     }

     em = (struct encap_msghdr *)buffer;

     if (!hmac) {
	  bzero(buffer, EMT_SETSPI_FLEN + 4 + ob->klen);

	  em->em_msglen = EMT_SETSPI_FLEN + AH_OLD_XENCAP_LEN + ob->klen;
	  
	  em->em_alg = XF_OLD_AH;

	  xdo = (struct ah_old_xencap *)(em->em_dat);
	  
	  xdo->amx_hash_algorithm = xf->kernel_id;
	  xdo->amx_keylen = ob->klen;
	
	  bcopy(secrets, xdo->amx_key, ob->klen);

     } else {
	  bzero(buffer, EMT_SETSPI_FLEN + AH_NEW_XENCAP_LEN + ob->klen);

	  em->em_msglen = EMT_SETSPI_FLEN + AH_NEW_XENCAP_LEN + ob->klen;

	  em->em_alg = XF_NEW_AH;

	  xdn = (struct ah_new_xencap *)(em->em_dat);

	  xdn->amx_hash_algorithm = xf->kernel_id;
	  xdn->amx_wnd = 16;
	  xdn->amx_keylen = ob->klen;

	  bcopy(secrets, xdn->amx_key, ob->klen);
     }
     
     em->em_version = PFENCAP_VERSION_1;
     em->em_type = EMT_SETSPI;
     em->em_spi = htonl((SPI->SPI[0]<<24) + (SPI->SPI[1]<<16) + 
			(SPI->SPI[2]<<8) + SPI->SPI[3]);
     em->em_src.s_addr = inet_addr(SPI->flags & SPI_OWNER ?
				   SPI->address : SPI->local_address);
     em->em_dst.s_addr = inet_addr(SPI->flags & SPI_OWNER ? 
				   SPI->local_address : SPI->address);
	  
     if (SPI->flags & SPI_TUNNEL) {
	  em->em_osrc.s_addr = inet_addr(SPI->flags & SPI_OWNER ?
					 SPI->address : SPI->local_address);
	  em->em_odst.s_addr = inet_addr(SPI->flags & SPI_OWNER ?
					 SPI->local_address : SPI->address);
     }
     em->em_sproto = IPPROTO_AH;

     kernel_debug(("kernel_ah: %08x. %s-Mode\n", 
		   em->em_spi, 
		   SPI->flags & SPI_TUNNEL ? "Tunnel" : "Transport"));

     if (!kernel_xf_set(em)) {
	  log_error(1, "kernel_xf_set() in kernel_ah()");
	  return -1;
     }
     return ob->klen;
}

int
kernel_esp(attrib_t *ob, attrib_t *ob2, struct spiob *SPI, u_int8_t *secrets)
{
     struct encap_msghdr *em;
     struct esp_old_xencap *xdo;
     struct esp_new_xencap *xdn;
     attrib_t *attenc, *attauth = NULL;
     u_int8_t *sec1, *sec2 = NULL;
     transform *xf_enc, *xf_auth;

     if (ob->type & AT_AUTH) {
	  if (ob2 == NULL || ob2->type != AT_ENC) {
	       log_error(0, "No encryption after auth given in kernel_esp()");
	       return -1;
	  }
	  attenc = ob2;
	  attauth = ob;
	  sec2 = secrets;
	  sec1 = secrets + ob->klen;
     } else if (ob->type == AT_ENC) {
	  attenc = ob;
	  sec1 = secrets;
	  if (ob2 != NULL && (ob2->type & AT_AUTH)) {
	       attauth = ob2;
	       sec2 = secrets + ob->klen;
	  }
     } else {
	  log_error(0, "No encryption transform given in kernel_esp()");
	  return -1;
     }

     xf_enc = kernel_get_transform(attenc->id);
     if ((xf_enc->flags & ESP_OLD) && attauth != NULL) {
	  log_error(0, "Old ESP does not support AH in kernel_esp()");
	  return -1;
     }

     if (attauth != NULL)
	  xf_auth = kernel_get_transform(attauth->id);

     em = (struct encap_msghdr *)buffer;

     if (xf_enc->flags & ESP_OLD) {
	  bzero(buffer, EMT_SETSPI_FLEN + ESP_OLD_XENCAP_LEN +4+attenc->klen);
	  
	  em->em_msglen = EMT_SETSPI_FLEN + ESP_OLD_XENCAP_LEN +4+attenc->klen;

	  em->em_alg = XF_OLD_ESP;

	  xdo = (struct esp_old_xencap *)(em->em_dat);

	  xdo->edx_enc_algorithm = xf_enc->kernel_id;
	  xdo->edx_ivlen = 4;
	  xdo->edx_keylen = attenc->klen;

	  bcopy(SPI->SPI, xdo->edx_data, 4);
	  bcopy(sec1, xdo->edx_data+4, attenc->klen);
     } else {
	  bzero(buffer, EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN + attenc->klen +
		(attauth ? attauth->klen : 0));

	  em->em_msglen = EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN + 
	       attenc->klen + (attauth ? attauth->klen : 0);

	  em->em_alg = XF_NEW_ESP;

	  xdn = (struct esp_new_xencap *)(em->em_dat);

	  xdn->edx_enc_algorithm = xf_enc->kernel_id;
	  xdn->edx_hash_algorithm = attauth ? xf_auth->kernel_id : 0;
	  xdn->edx_ivlen = 0;
	  xdn->edx_confkeylen = attenc->klen;
	  xdn->edx_authkeylen = attauth ? attauth->klen : 0;
	  xdn->edx_wnd = 16;
	  xdn->edx_flags = attauth ? ESP_NEW_FLAG_AUTH : 0;

	  bcopy(sec1, xdn->edx_data, attenc->klen);
	  if (attauth != NULL)
	       bcopy(sec2, xdn->edx_data + attenc->klen, attauth->klen);
     }
     /* Common settings shared by ESP_OLD and ESP_NEW */

     em->em_version = PFENCAP_VERSION_1;
     em->em_type = EMT_SETSPI;
     em->em_spi = htonl((SPI->SPI[0]<<24) + (SPI->SPI[1]<<16) + 
			(SPI->SPI[2]<<8) + SPI->SPI[3]);
     em->em_src.s_addr = inet_addr(SPI->flags & SPI_OWNER ? 
				   SPI->address : SPI->local_address);
     em->em_dst.s_addr = inet_addr(SPI->flags & SPI_OWNER ?
				   SPI->local_address : SPI->address);
     em->em_sproto = IPPROTO_ESP;
	
     if (SPI->flags & SPI_TUNNEL) {
	  em->em_osrc.s_addr = inet_addr(SPI->flags & SPI_OWNER ? 
					 SPI->address : SPI->local_address);
	  em->em_odst.s_addr = inet_addr(SPI->flags & SPI_OWNER ?
					 SPI->local_address : SPI->address);
     }

     kernel_debug(("kernel_esp: %08x. %s-Mode\n", 
		   em->em_spi,
		   SPI->flags & SPI_TUNNEL ? "Tunnel" : "Transport"));
     
     if (!kernel_xf_set(em)) {
	  log_error(1, "kernel_xf_set() in kernel_esp()");
	  return -1;
     }
     
     return attenc->klen + (attauth ? attauth->klen : 0);
}

/* Group an ESP SPI with an AH SPI */

int
kernel_group_spi(char *address, u_int8_t *spi)
{
     struct encap_msghdr *em;
     in_addr_t addr;
     u_int32_t SPI;

     SPI = (spi[0]<<24) + (spi[1]<<16) + (spi[2]<<8) + spi[3];

     kernel_debug(("kernel_group_spi: %s, %08x\n", address, SPI));

     addr = inet_addr(address);

     bzero(buffer, EMT_GRPSPIS_FLEN);

     em = (struct encap_msghdr *)buffer;
	  
     em->em_msglen = EMT_GRPSPIS_FLEN;
     em->em_version = PFENCAP_VERSION_1;
     em->em_type = EMT_GRPSPIS;

     em->em_rel_spi = htonl(SPI);
     em->em_rel_dst.s_addr = addr;
     em->em_rel_sproto = IPPROTO_ESP;
     em->em_rel_spi2 = htonl(SPI);
     em->em_rel_dst2.s_addr = addr;
     em->em_rel_sproto2 = IPPROTO_AH;
     
     if (!kernel_xf_set(em)) {
	  log_error(1, "kernel_xf_set() in kernel_group_spi()");
	  return -1;
     }

     return 1;
}

int
kernel_enable_spi(in_addr_t isrc, in_addr_t ismask, 
		  in_addr_t idst, in_addr_t idmask, 
		  char *address, u_int8_t *spi, int proto, int flags)
{
     struct encap_msghdr *em;
     u_int32_t SPI;

     SPI = (spi[0]<<24) + (spi[1]<<16) + (spi[2]<<8) + spi[3];

     kernel_debug(("kernel_enable_spi: %08x\n", SPI));

     bzero(buffer, EMT_ENABLESPI_FLEN);

     em = (struct encap_msghdr *)buffer;
     
     em->em_msglen = EMT_ENABLESPI_FLEN;
     em->em_version = PFENCAP_VERSION_1;
     em->em_type = EMT_ENABLESPI;

     em->em_ena_isrc.s_addr = isrc;
     em->em_ena_ismask.s_addr = ismask;
     em->em_ena_idst.s_addr = idst;
     em->em_ena_idmask.s_addr = idmask;

     em->em_ena_dst.s_addr = inet_addr(address);
     em->em_ena_spi = htonl(SPI);
     em->em_ena_sproto = proto;
     em->em_ena_flags = flags;

     if (!kernel_xf_set(em)) {
	  log_error(1, "kernel_xf_set() in kernel_enable_spi()");
	  return -1;
     }

     return 1;
}

int
kernel_disable_spi(in_addr_t isrc, in_addr_t ismask, 
		   in_addr_t idst, in_addr_t idmask, 
		   char *address, u_int8_t *spi, int proto, int flags)
{
     struct encap_msghdr *em;
     u_int32_t SPI;

     SPI = (spi[0]<<24) + (spi[1]<<16) + (spi[2]<<8) + spi[3];

     kernel_debug(("kernel_disable_spi: %08x\n", SPI));

     bzero(buffer, EMT_DISABLESPI_FLEN);

     em = (struct encap_msghdr *)buffer;
     
     em->em_msglen = EMT_DISABLESPI_FLEN;
     em->em_version = PFENCAP_VERSION_1;
     em->em_type = EMT_DISABLESPI;

     em->em_ena_isrc.s_addr = isrc;
     em->em_ena_ismask.s_addr = ismask;
     em->em_ena_idst.s_addr = idst;
     em->em_ena_idmask.s_addr = idmask;

     em->em_ena_dst.s_addr = inet_addr(address);
     em->em_ena_spi = htonl(SPI);
     em->em_ena_sproto = proto;
     em->em_ena_flags = flags;
     
     if (!kernel_xf_set(em) && errno != ENOENT) {
	  log_error(1, "kernel_xf_set() in kernel_disable_spi()");
	  return -1;
     }

     return 1;
}

/*
 * Remove a single SPI from the kernel database.
 */

int
kernel_delete_spi(char *address, u_int8_t *spi, int proto)
{
	struct encap_msghdr *em;

	bzero(buffer, EMT_DELSPI_FLEN);

	em = (struct encap_msghdr *)buffer;

	em->em_msglen = EMT_DELSPI_FLEN;
	em->em_version = PFENCAP_VERSION_1;
	em->em_type = EMT_DELSPI;
	em->em_gen_spi = htonl((spi[0]<<24) + (spi[1]<<16) + 
			   (spi[2]<<8) + spi[3]);
	em->em_gen_dst.s_addr = inet_addr(address);
	em->em_gen_sproto = proto;

	kernel_debug(("kernel_delete_spi: %08x\n", em->em_gen_spi));

	if (!kernel_xf_set(em)) {
	     log_error(1, "kernel_xf_set() in kernel_delete_spi()");
	     return -1;
	}

	return 1;
}

/*
 * Creates the correspondings SPI's with the kernel and establishes
 * routing if necessary, i.e. when the SPIs were not created by
 * kernel notifies.
 */

int
kernel_insert_spi(struct stateob *st, struct spiob *SPI)
{
     u_int8_t *spi;
     u_int8_t *attributes;
     u_int16_t attribsize, ahsize, espsize;
     u_int8_t *secrets, *ah, *esp;
     attrib_t *attprop;
     int offset, proto = 0;

     spi = SPI->SPI;
     attributes = SPI->attributes;
     attribsize = SPI->attribsize;
     secrets = SPI->sessionkey;

     get_attrib_section(attributes, attribsize, &esp, &espsize,
			AT_ESP_ATTRIB);
     get_attrib_section(attributes, attribsize, &ah, &ahsize,
			AT_AH_ATTRIB);

     if (esp != NULL) {
	  int count = 0;
	  attrib_t *atesp = NULL, *atah = NULL;

	  while (count < espsize && (atesp == NULL || atah == NULL)) {
	       if ((attprop = getattrib(esp[count])) == NULL) {
		    log_error(0, "Unknown attribute %d for ESP in kernel_insert_spi()",
			      esp[count]);
		    return -1;
	       }
	       if (atesp == NULL && attprop->type == AT_ENC)
		    atesp = attprop;
	       else if(atah == NULL && (attprop->type & AT_AUTH))
		    atah = attprop;

	       count += esp[count+1]+2;
	  }
	  if (atesp == NULL) {
	       log_error(0, "No encryption attribute in ESP section for SA(%08x, %s->%s) in kernel_insert()", (SPI->SPI[0] << 24) + (SPI->SPI[1] << 16) + (SPI->SPI[2] << 8) + SPI->SPI[3], SPI->local_address, SPI->address);
	       return -1;
	  }

	  if (vpn_mode)
	       SPI->flags |= SPI_TUNNEL;
     
	  offset = kernel_esp(atesp, atah, SPI, secrets);
	  if (offset == -1)
	       return -1;
	  secrets += offset;
     }

     if (ah != NULL) {
	  int count = 0, hmac = 0;
	  attrib_t *atah = NULL;

	  while (count < ahsize) {
	       if ((attprop = getattrib(ah[count])) == NULL) {
		    log_error(0, "Unknown attribute %d for AH in kernel_insert_spi()",
			      ah[count]);
		    return -1;
	       }
	       if(atah == NULL && (attprop->type & AT_AUTH))
		    atah = attprop;
	       else if (attprop->type == 0) {
		    switch (attprop->id) {
		    case AT_HMAC:
			 hmac = 1;
			 break;
		    default:
			 break;
		    }
	       }

	       count += ah[count+1]+2;
	  }

	  if (atah == NULL) {
	       log_error(0, "No authentication attribute in AH section for SA(%08x, %s->%s) in kernel_insert()", (SPI->SPI[0] << 24) + (SPI->SPI[1] << 16) + (SPI->SPI[2] << 8) + SPI->SPI[3], SPI->local_address, SPI->address);
	       return -1;
	  }

	  if (vpn_mode && esp == NULL)
	       SPI->flags |= SPI_TUNNEL;
	  else 
	       SPI->flags &= ~SPI_TUNNEL;

	  offset = kernel_ah(atah, SPI, secrets, hmac);
	  if (offset == -1)
	       return -1;
	  secrets += offset; 
     }

     if (esp != NULL)
	  proto = IPPROTO_ESP;
     else
	  proto = IPPROTO_AH;

     /* Group the SPIs for User */
     if (!(SPI->flags & SPI_OWNER) && ah != NULL && esp != NULL) {
	  if (kernel_group_spi(SPI->address, spi) == -1)
	       log_error(0, "kernel_group_spi() in kernel_insert_spi()");
     }
     
     if (!(SPI->flags & SPI_OWNER)) 
	  if (!(SPI->flags & SPI_NOTIFY) || vpn_mode) {
	       if (kernel_enable_spi(SPI->isrc, SPI->ismask,
				     SPI->idst, SPI->idmask,
				     SPI->address, spi, proto, 
				     ENABLE_FLAG_REPLACE|ENABLE_FLAG_LOCAL |
				     (vpn_mode ? ENABLE_FLAG_MODIFY : 0)) == -1)
		    log_error(0, "kernel_enable_spi() in kernel_insert_spi()");
	  } else {
	       /* 
		* Inform the kernel that we obtained the requested SA
		*/
	       kernel_notify_result(st, SPI, proto);
	  }
	  
     /* Is this what people call perfect forward security ? */
     bzero(SPI->sessionkey, SPI->sessionkeysize);
     free(SPI->sessionkey);
     SPI->sessionkey = NULL; SPI->sessionkeysize = 0;

     return 1;
}

/*
 * Deletes an SPI object, which means removing the SPIs from the
 * kernel database and the deletion of all routes which were
 * established on our behalf. Routes for SA's which were created by
 * kernel notifies also get removed, since they are not any longer
 * valid anyway.
 */

int
kernel_unlink_spi(struct spiob *ospi)
{
     int n, proto = 0;
     attrib_t *attprop;
     u_int32_t spi;
     u_int8_t *p, *ah, *esp;
     u_int16_t ahsize, espsize;

     if (!(ospi->flags & SPI_OWNER))
	  p = ospi->address;
     else
	  p = ospi->local_address;
     
     get_attrib_section(ospi->attributes, ospi->attribsize, &esp, &espsize,
			AT_ESP_ATTRIB);
     get_attrib_section(ospi->attributes, ospi->attribsize, &ah, &ahsize,
			AT_AH_ATTRIB);

     if (esp != NULL) {
	  int flag = (vpn_mode ? ENABLE_FLAG_MODIFY : 0) | ENABLE_FLAG_LOCAL;
	  if (!(ospi->flags & SPI_OWNER) && 
	      kernel_disable_spi(ospi->isrc, ospi->ismask,
				 ospi->idst, ospi->idmask,
				 ospi->address, ospi->SPI, 
				 IPPROTO_ESP, flag) == -1)
	       log_error(0, "kernel_disable_spi() in kernel_unlink_spi()");

	  if (kernel_delete_spi(p, ospi->SPI, IPPROTO_ESP) == -1)
	       log_error(0, "kernel_delete_spi() in kernel_unlink_spi()");
     }
	  
     if (ah != NULL) {
	  if (esp == NULL) {
	       int flag = (vpn_mode ? ENABLE_FLAG_MODIFY : 0) | 
		    ENABLE_FLAG_LOCAL;
	       if (!(ospi->flags & SPI_OWNER) &&
		   kernel_disable_spi(ospi->isrc, ospi->ismask,
				      ospi->idst, ospi->idmask,
				      ospi->address, ospi->SPI, 
				      IPPROTO_AH, flag) == -1)
		    log_error(0, "kernel_disable_spi() in kernel_unlink_spi()");
	  }
	  
	  if (kernel_delete_spi(p, ospi->SPI, IPPROTO_AH) == -1)
	       log_error(0, "kernel_delete_spi() in kernel_unlink_spi()");
     }

     return 1;
}

/*
 * Handles Notifies from the kernel, which can include Requests for new
 * SAs, soft and hard expirations for already established SAs.
 */

void
kernel_handle_notify(int sd)
{
     struct encap_msghdr em;
     int msglen;

     if ((msglen = recvfrom(sd, (char *)&em, sizeof(em),0, NULL,0)) == -1) {
	  log_error(1, "recvfrom() in kernel_handle_notify()");
	  return;
     }

     if (msglen != em.em_msglen) {
	  log_error(0, "message length incorrect in kernel_handle_notify(): got %d where it should be %d", msglen, em.em_msglen);
	  return;
     }

     if (em.em_type != EMT_NOTIFY) {
	  log_error(0, "message type is not notify in kernel_handle_notify()");
	  return;
     }

#ifdef DEBUG
     printf("Received EMT_NOTIFY message: subtype %d\n", em.em_not_type);
#endif

     switch (em.em_not_type) {
     case NOTIFY_SOFT_EXPIRE:
     case NOTIFY_HARD_EXPIRE:
	  log_error(0, "Notify is an SA Expiration - not yet supported.\n");
	  return;
     case NOTIFY_REQUEST_SA:
#ifdef DEBUG
	  printf("Notify SA Request for IP: %s, require %d\n",
		 inet_ntoa(em.em_not_dst), em.em_not_satype);
#endif
	  kernel_request_sa(&em);
	  break;
     default:
	  log_error(0, "Unknown notify message in kernel_handle_notify");
	  return;
     }
}

/*
 * Tries to establish a new SA according to the information in a 
 * REQUEST_SA notify message received from the kernel.
 */

int
kernel_request_sa(struct encap_msghdr *em) 
{
     struct stateob *st;
     time_t tm;
     char *address = inet_ntoa(em->em_not_dst);

     /* Try to find an already established exchange which is still valid */
     st = state_find(address);

     tm = time(NULL);
     while (st != NULL && (st->lifetime <= tm || st->phase >= SPI_UPDATE))
	  st = state_find_next(st, address);

     if (st == NULL) {
	  /* No established exchange found, start a new one */
	  if ((st = state_new()) == NULL) {
	       log_error(0, "state_new() failed in kernel_request_sa() for remote ip %s",
			 address);
	       return (-1);
	  }
	  /* Set up the state information */
	  strncpy(st->address, address, sizeof(st->address)-1);
	  st->port = global_port;
	  st->sport = em->em_not_sport;
	  st->dport = em->em_not_dport;
	  st->protocol = em->em_not_protocol;

	  /*
	   * For states which were created by kernel notifies we wont
	   * set up routes since other keying daemons might habe beaten
	   * us in establishing SAs. The kernel has to decide which SA
	   * will actually be routed.
	   */
	  st->flags = IPSEC_NOTIFY;
	  if (em->em_not_satype & NOTIFY_SATYPE_CONF)
	       st->flags |= IPSEC_OPT_ENC;
	  if (em->em_not_satype & NOTIFY_SATYPE_AUTH)
	       st->flags |= IPSEC_OPT_AUTH;
	  /* XXX - handling of tunnel requests missing */
	  if (start_exchange(global_socket, st, st->address, st->port) == -1) {
	       log_error(0, "start_exchange() in kernel_request_sa() - informing kernel of failure");
	       /* Inform kernel of our failure */
	       kernel_notify_result(st, NULL, 0);
	       state_value_reset(st);
	       free(st);
	       return (-1);
	  } else
	       state_insert(st);
     } else {
	  /* 
	   * We need different attributes for this exchange, send
	   * an SPI_NEEDED message.
	   */
     }
}

/*
 * Report the established SA or either our failure to create an SA
 * to the kernel.
 * Passing a SPI of NULL means failure.
 */

void
kernel_notify_result(struct stateob *st, struct spiob *spi, int proto)
{
     struct encap_msghdr em;

     bzero((char *)&em, sizeof(em));
     em.em_type = EMT_NOTIFY;
     em.em_msglen = EMT_NOTIFY_FLEN;
     em.em_version = PFENCAP_VERSION_1;
     em.em_not_type = NOTIFY_REQUEST_SA;
     if (spi != NULL) {
	  em.em_not_spi = htonl((spi->SPI[0]<<24) + (spi->SPI[1]<<16) + 
				(spi->SPI[2]<<8) + spi->SPI[3]);
	  em.em_not_dst.s_addr = inet_addr(spi->address);
	  em.em_not_src.s_addr = inet_addr(spi->local_address);
	  em.em_not_sproto = proto;
     } 
     if (st != NULL) {
	  em.em_not_dst.s_addr = inet_addr(st->address);
	  em.em_not_sport = st->sport;
	  em.em_not_dport = st->dport;
	  em.em_not_protocol = st->protocol;
     }

     if (!kernel_xf_set(&em))
	  log_error(1, "kernel_xf_set() in kernel_notify_result()");
}
