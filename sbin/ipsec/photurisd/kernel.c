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

#ifndef lint
static char rcsid[] = "$Id: kernel.c,v 1.4 1997/09/14 10:37:51 deraadt Exp $";
#endif

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
#include "state.h"
#include "attributes.h"
#include "buffer.h"
#include "spi.h"
#include "kernel.h"
#include "errlog.h"
#ifdef DEBUG
#include "config.h"
#endif

static int sd;

int
init_kernel(void)
{
     if ((sd = socket(AF_ENCAP, SOCK_RAW, AF_UNSPEC)) < 0) 
	  crit_error(1, "socket() in init_kernel()");
     return 1;
}

int
kernel_get_socket(void)
{
     return sd;
}

int
kernel_xf_set(struct encap_msghdr *em)
{
     if (write(sd, (char *)em, em->em_msglen) != em->em_msglen) {
	  log_error(1, "write() in kernel_xf_set()");
	  return 0;
     }
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

     bzero(buffer, EMT_ENABLESPI_FLEN);

     em = (struct encap_msghdr *)buffer;
     
     em->em_msglen = EMT_RESERVESPI_FLEN;
     em->em_version = PFENCAP_VERSION_1;
     em->em_type = EMT_RESERVESPI;

     em->em_gen_spi = spi;
     em->em_gen_dst.s_addr = inet_addr(srcaddress);
     em->em_gen_sproto = proto;
     
     if (!kernel_xf_set(em))
	  return 0;

     if (!kernel_xf_read(em, EMT_RESERVESPI_FLEN))
	  return 0;

     return em->em_gen_spi;
}

int
kernel_md5(char *srcaddress, char *dstaddress, u_int8_t *spi, u_int8_t *secret,
	   int tunnel)
{
     struct encap_msghdr *em;
     struct ah_old_xencap *xd;
     int klen;

     klen = MD5_KEYLEN/8;

     bzero(buffer, EMT_SETSPI_FLEN + 4 + klen);

     em = (struct encap_msghdr *)buffer;
	
     em->em_msglen = EMT_SETSPI_FLEN + AH_OLD_XENCAP_LEN + klen;
     em->em_version = PFENCAP_VERSION_1;
     em->em_type = EMT_SETSPI;
     em->em_spi = htonl((spi[0]<<24) + (spi[1]<<16) + 
			(spi[2]<<8) + spi[3]);
     em->em_src.s_addr = inet_addr(srcaddress);
     em->em_dst.s_addr = inet_addr(dstaddress);

     if (tunnel) {
	  em->em_osrc.s_addr = inet_addr(srcaddress);
	  em->em_odst.s_addr = inet_addr(dstaddress);
     }

     em->em_alg = XF_OLD_AH;
     em->em_sproto = IPPROTO_AH;

     xd = (struct ah_old_xencap *)(em->em_dat);

     xd->amx_hash_algorithm = ALG_AUTH_MD5;
     xd->amx_keylen = klen;
	
     bcopy(secret, xd->amx_key, klen);

     if (!kernel_xf_set(em))
	  return -1;

     return MD5_KEYLEN/8;
}

int
kernel_des(char *srcaddress, char *dstaddress, u_int8_t *spi, u_int8_t *secret,
	   int tunnel)
{
     struct encap_msghdr *em;
     struct esp_old_xencap *xd;

     bzero(buffer, EMT_SETSPI_FLEN + ESP_OLD_XENCAP_LEN + 4 + 8);

     em = (struct encap_msghdr *)buffer;
	
     em->em_msglen = EMT_SETSPI_FLEN + ESP_OLD_XENCAP_LEN + 4 + 8;
     em->em_version = PFENCAP_VERSION_1;
     em->em_type = EMT_SETSPI;
     em->em_spi = htonl((spi[0]<<24) + (spi[1]<<16) + 
			(spi[2]<<8) + spi[3]);
     em->em_src.s_addr = inet_addr(srcaddress);
     em->em_dst.s_addr = inet_addr(dstaddress);
	
     if (tunnel) {
	  em->em_osrc.s_addr = inet_addr(srcaddress);
	  em->em_odst.s_addr = inet_addr(dstaddress);
     }

     em->em_alg = XF_OLD_ESP;
     em->em_sproto = IPPROTO_ESP;

     xd = (struct esp_old_xencap *)(em->em_dat);

     xd->edx_enc_algorithm = ALG_ENC_DES;
     xd->edx_ivlen = 4;
     xd->edx_keylen = 8;

     bcopy(spi, xd->edx_data, 4);
     bcopy(secret, xd->edx_data + 8, 8);
     
     if (!kernel_xf_set(em))
	  return -1;
     
     return 8;
}

/* Group an ESP SPI with an AH SPI */

int
kernel_group_spi(char *address, u_int8_t *spi)
{
     struct encap_msghdr *em;
     in_addr_t addr;
     u_int32_t SPI;

     SPI = (spi[0]<<24) + (spi[1]<<16) + (spi[2]<<8) + spi[3];
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
     
     if (!kernel_xf_set(em))
	  return -1;

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

     if (!kernel_xf_set(em))
	  return -1;

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
     
     if (!kernel_xf_set(em))
	  return -1;

     return 1;
}

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

	if (!kernel_xf_set(em))
	     return -1;

	return 1;
}

int
kernel_insert_spi(struct spiob *SPI)
{
     u_int8_t *spi;
     u_int8_t *attributes;
     u_int16_t attribsize;
     u_int8_t *secrets;
     int i, n, offset, proto = 0;

     spi = SPI->SPI;
     attributes = SPI->attributes;
     attribsize = SPI->attribsize;
     secrets = SPI->sessionkey;
     
     for(n=0, i=0; n<attribsize; n += attributes[n+1] + 2) {
	  switch(attributes[n]) {
	  case AT_AH_ATTRIB:
	  case AT_ESP_ATTRIB:
	       break;
	  case AT_MD5_KDP:
	       offset = kernel_md5(SPI->local_address, SPI->flags & SPI_OWNER ? 
				   SPI->local_address : SPI->address,
				   spi, secrets, SPI->flags & SPI_TUNNEL);
	       if (offset == -1)
		    return -1;
	       secrets += offset; 
	       i++;
	       if (!proto) 
		    proto = IPPROTO_AH;
	       break;
	  case AT_DES_CBC:
	       offset = kernel_des(SPI->local_address, SPI->flags & SPI_OWNER ? 
				   SPI->local_address : SPI->address,
				   spi, secrets, SPI->flags & SPI_TUNNEL);
	       if (offset == -1)
		    return -1;
	       secrets += offset;
	       i++;
	       if (!proto)
		    proto = IPPROTO_ESP;
	       break;
	  default:
	       log_error(0, "Unknown attribute %d in kernel_insert_spi()",
			 attributes[n]);
	       return -1;
	  }
     }

     /* Group the SPIs for User */
     if (!(SPI->flags & SPI_OWNER) && i > 1) {
	  if (kernel_group_spi(SPI->address, spi) == -1)
	       log_error(0, "kernel_group_spi() in kernel_insert_spi()");
     }
     
     if (!(SPI->flags & SPI_OWNER) && !(SPI->flags & SPI_NOTIFY)) {
	  if (kernel_enable_spi(SPI->isrc, SPI->ismask,
				SPI->idst, SPI->idmask,
				SPI->address, spi, proto, 
				ENABLE_FLAG_REPLACE|ENABLE_FLAG_LOCAL) == -1)
	       log_error(0, "kernel_enable_spi() in kernel_insert_spi()");
     }
	  
     /* Is this what people call perfect forward security ? */
     bzero(SPI->sessionkey, SPI->sessionkeysize);
     free(SPI->sessionkey);
     SPI->sessionkey = NULL; SPI->sessionkeysize = 0;

     return 1;
}

int
kernel_unlink_spi(struct spiob *ospi)
{
     int n, proto = 0;
     u_int32_t spi;
     u_int8_t SPI[SPI_SIZE], *p;

     if (!(ospi->flags & SPI_OWNER))
	  p = ospi->address;
       else
	  p = ospi->local_address;
     

     spi = (ospi->SPI[0]<<24) + (ospi->SPI[1]<<16) +
	  (ospi->SPI[2]<<8) + ospi->SPI[3];
	  
     for(n=0; n<ospi->attribsize; n += ospi->attributes[n+1] + 2) {
	  SPI[0] = (spi >> 24) & 0xFF;
	  SPI[1] = (spi >> 16) & 0xFF;
	  SPI[2] = (spi >> 8)  & 0xFF;
	  SPI[3] =  spi        & 0xFF;
	  switch(ospi->attributes[n]) {
	  case AT_AH_ATTRIB:
	  case AT_ESP_ATTRIB:
	       break;
	  case AT_MD5_KDP:
	       if (!proto) {
		    proto = IPPROTO_AH;
		    if (!(ospi->flags & SPI_OWNER) &&
			kernel_disable_spi(ospi->isrc, ospi->ismask,
					   ospi->idst, ospi->idmask,
					   ospi->address, ospi->SPI, proto,
					   ENABLE_FLAG_LOCAL) == -1)
			 log_error(0, "kernel_disable_spi() in kernel_unlink_spi()");
	       }

	       if (kernel_delete_spi(p, SPI, IPPROTO_AH) == -1)
		    log_error(0, "kernel_delete_spi() in kernel_unlink_spi()");
	       break;
	  case AT_DES_CBC:
	       if (!proto) {
		    proto = IPPROTO_ESP;
		    if (!(ospi->flags & SPI_OWNER) && 
			kernel_disable_spi(ospi->isrc, ospi->ismask,
					   ospi->idst, ospi->idmask,
					   ospi->address, ospi->SPI, proto,
					   ENABLE_FLAG_LOCAL) == -1)
			 log_error(0, "kernel_disable_spi() in kernel_unlink_spi()");
	       }
	       if (kernel_delete_spi(p, SPI, IPPROTO_ESP) == -1)
		    log_error(0, "kernel_delete_spi() in kernel_unlink_spi()");
	       break;
	  default:
	       log_error(0, "Unknown attribute %d in kernel_unlink_spi()",
			 ospi->attributes[n]);
	       return -1;
	  }
     }


     return 1;
}
