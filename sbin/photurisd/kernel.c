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
static char rcsid[] = "$Id: kernel.c,v 1.3 1999/03/31 20:33:45 niklas Exp $";
#endif

#include <time.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

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
#include <net/pfkeyv2.h>
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

#define SPITOINT(x) (((x)[0]<<24) + ((x)[1]<<16) + ((x)[2]<<8) + (x)[3])
#define KERNEL_XF_SET(x) kernel_xf_set(sd, buffer, BUFFER_SIZE, iov, cnt, x)

static int sd;		/* normal PFKEY socket */
static int regsd;	/* PFKEY socket for Register and Acquire */
static int pfkey_seq;
static pid_t pfkey_pid;

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

/*
 * Mark a transform as supported by the kernel
 */

void
kernel_transform_seen(int id, int type)
{
     int i;

     for (i=sizeof(xf)/sizeof(transform)-1; i >= 0; i--) 
	  if (xf[i].kernel_id == id && (xf[i].flags & type)) {
	       xf[i].flags |= XF_SUP;
	       return;
	  }
}

/*
 * See if we know about this transform and if it is supported
 * by the kernel.
 */

int
kernel_known_transform(int id)
{
     transform *xf = kernel_get_transform(id);

     return (xf == NULL || !(xf->flags & XF_SUP)) ? -1 : 0;
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
     if ((sd = socket(PF_KEY, SOCK_RAW, PF_KEY_V2)) == -1) 
	  crit_error(1, "socket(PF_KEY) for IPSec keyengine in init_kernel()");
     if ((regsd = socket(PF_KEY, SOCK_RAW, PF_KEY_V2)) == -1) 
	  crit_error(1, "socket() for PFKEY register in init_kernel()");

     pfkey_seq = 0;
     pfkey_pid = getpid();

     if (kernel_register(regsd) == -1)
	  crit_error(0, "PFKEY socket registration failed in init_kernel()");
     
     return 1;
}

int
kernel_get_socket(void)
{
     return regsd;
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
kernel_xf_set(int sd, char *buffer, int blen, struct iovec *iov,
	      int cnt, int len)
{
     struct sadb_msg *sres;
     int seq;

     sres = (struct sadb_msg *)iov[0].iov_base;
     seq = sres->sadb_msg_seq;

     if (writev(sd, iov, cnt) != len) {
	  perror("writev() in kernel_xf_set()");
	  return 0;
     }

     if (buffer)
	  return kernel_xf_read(sd, buffer, blen, seq);
     return 1;
}

int
kernel_xf_read(int sd, char *buffer, int blen, int seq)
{
     struct sadb_msg *sres = (struct sadb_msg *)buffer;
     int len;

     /*
      * Read in response from the kernel. If seq number and/or PID are
      * given, we need to check PID and sequence number to see if it
      * really is a message for us.
      */
     do {
	  if (recv(sd, sres, sizeof(*sres), MSG_PEEK) != sizeof(*sres)) {
	       perror("read() in kernel_xf_read()");
	       return 0;
	  }
	  len = sres->sadb_msg_len * 8;
	  if (len >= BUFFER_SIZE) {
	       log_error(0, "PFKEYV2 message len %d too big in kernel_xf_read()", len);
	       return 0;
	  }
	  if (read(sd, sres, len) != len) {
	       perror("read() in kernel_xf_read()");
	       return 0;
	  }
     } while ((seq && sres->sadb_msg_seq != seq) ||
	      (sres->sadb_msg_pid && sres->sadb_msg_pid != pfkey_pid));
     if (sres->sadb_msg_errno) {
	  log_error(0, "kernel_xf_read: PFKEYV2 result: %s",
		    strerror(sres->sadb_msg_errno));
	  return 0;
     }
     return 1;
}

int
kernel_register(int sd)
{
     struct sadb_msg smsg, *sres;
     struct sadb_supported *ssup;
     struct sadb_alg *salg;
     int len;
     struct iovec iov[1];
     int cnt = 0;

     kernel_debug(("kernel_register: fd %d\n", sd));

     bzero(&smsg, sizeof(smsg));

     smsg.sadb_msg_len = sizeof(smsg) / 8;
     smsg.sadb_msg_version = PF_KEY_V2;
     smsg.sadb_msg_seq = pfkey_seq++;
     smsg.sadb_msg_pid = pfkey_pid;
     smsg.sadb_msg_type = SADB_REGISTER;
     iov[cnt].iov_base = &smsg;
     iov[cnt++].iov_len = sizeof(smsg);

     /* Register for AH */
     smsg.sadb_msg_satype = SADB_SATYPE_ESP;
     if (!kernel_xf_set(regsd, buffer, BUFFER_SIZE, iov, cnt,
			smsg.sadb_msg_len*8)) {
	  log_error(1, "kernel_xf_set() in kernel_reserve_single_spi()");
	  return -1;
     }

     /* Register for ESP */
     smsg.sadb_msg_satype = SADB_SATYPE_AH;
     smsg.sadb_msg_seq = pfkey_seq++;
     if (!kernel_xf_set(regsd, buffer, BUFFER_SIZE, iov, cnt,
			smsg.sadb_msg_len*8)) {
	  log_error(1, "kernel_xf_set() in kernel_reserve_single_spi()");
	  return -1;
     }

     /* 
      * XXX - this might need changing in the case that the response
      * to register only includes the transforms matching the satype
      * in the message.
      */
     sres = (struct sadb_msg *)buffer;
     ssup = (struct sadb_supported *)(sres + 1);
     if (ssup->sadb_supported_exttype != SADB_EXT_SUPPORTED) {
	  log_error(0, "SADB_REGISTER did not return a SADB_EXT_SUPORTED "
		    "struct: %d in kernel_register()",
		    ssup->sadb_supported_exttype);
	  return -1;
     }

     len = ssup->sadb_supported_len * 8 - sizeof(*ssup);
     if (len != (ssup->sadb_supported_nauth + ssup->sadb_supported_nencrypt) *
	 sizeof(struct sadb_alg)) {
	  log_error(0, "SADB_SUPPORTED length mismatch in kernel_register()");
	  return -1;
     }

     salg = (struct sadb_alg *)(ssup + 1);
     for (cnt = 0; cnt < ssup->sadb_supported_nauth; cnt++, salg++)
	  kernel_transform_seen(salg->sadb_alg_type, XF_AUTH);
     for (cnt = 0; cnt < ssup->sadb_supported_nencrypt; cnt++, salg++)
	  kernel_transform_seen(salg->sadb_alg_type, XF_ENC);
	  
     return 0;
}

u_int32_t
kernel_reserve_spi(char *src, char *dst, int options)
{
     u_int32_t spi;
     int proto;

     kernel_debug(("kernel_reserve_spi: %s\n", src));

     if ((options & (IPSEC_OPT_ENC|IPSEC_OPT_AUTH)) != 
	 (IPSEC_OPT_ENC|IPSEC_OPT_AUTH)) {
	  switch(options & (IPSEC_OPT_ENC|IPSEC_OPT_AUTH)) {
	  case IPSEC_OPT_ENC:
	       proto = IPPROTO_ESP;
	  default:
	       proto = IPPROTO_AH;
	  }
	  return kernel_reserve_single_spi(src, dst, 0, proto);
     }

     if (!(spi = kernel_reserve_single_spi(src, dst, 0, IPPROTO_ESP)))
	  return spi;
     
     /* Try to get the same spi for ah and esp */
     while (!kernel_reserve_single_spi(src, dst, spi, IPPROTO_AH)) {
	  kernel_delete_spi(src, spi, IPPROTO_ESP);
	  if (!(spi = kernel_reserve_single_spi(src, dst, 0, IPPROTO_ESP)))
	       return spi;
     }

     return spi;
}

u_int32_t
kernel_reserve_single_spi(char *srcaddress, char *dstaddress, u_int32_t spi,
			  int proto)
{
     struct sadb_msg smsg, *sres;
     struct sadb_address sad1, sad2; /* src and dst */
     struct sadb_spirange sspi;
     struct sadb_sa *ssa;
     union sockaddr_union src, dst;
     struct iovec iov[6];
     int cnt = 0;

     kernel_debug(("kernel_reserve_single_spi: %s, %08x\n", srcaddress, spi));

     bzero(&src, sizeof(union sockaddr_union));
     bzero(&dst, sizeof(union sockaddr_union));
     bzero(iov, sizeof(iov));

     bzero(&smsg, sizeof(smsg));
     bzero(&sad1, sizeof(sad1));
     bzero(&sad2, sizeof(sad2));
     bzero(&sspi, sizeof(sspi));

     smsg.sadb_msg_len = sizeof(smsg) / 8;
     smsg.sadb_msg_version = PF_KEY_V2;
     smsg.sadb_msg_seq = pfkey_seq++;
     smsg.sadb_msg_pid = pfkey_pid;
     smsg.sadb_msg_type = SADB_GETSPI;
     smsg.sadb_msg_satype = proto == IPPROTO_AH ? 
	  SADB_SATYPE_AH : SADB_SATYPE_ESP;
     iov[cnt].iov_base = &smsg;
     iov[cnt++].iov_len = sizeof(smsg);

     /* Source Address */
     sad1.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
     sad1.sadb_address_len = (sizeof(sad1) + sizeof(struct sockaddr_in)) / 8;
     iov[cnt].iov_base = &sad1;
     iov[cnt++].iov_len = sizeof(sad1);

     src.sin.sin_family = AF_INET;
     src.sin.sin_len = sizeof(struct sockaddr_in);
     src.sin.sin_addr.s_addr = inet_addr(dstaddress);

     iov[cnt].iov_base = &src;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);
     smsg.sadb_msg_len += sad1.sadb_address_len;

     /* Destination Address */
     sad2.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
     sad2.sadb_address_len = (sizeof(sad2) + sizeof(struct sockaddr_in)) / 8;

     iov[cnt].iov_base = &sad2;
     iov[cnt++].iov_len = sizeof(sad2);

     dst.sin.sin_family = AF_INET;
     dst.sin.sin_len = sizeof(struct sockaddr_in);
     dst.sin.sin_addr.s_addr = inet_addr(srcaddress);

     iov[cnt].iov_base = &dst;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);
     smsg.sadb_msg_len += sad2.sadb_address_len;

     sspi.sadb_spirange_exttype = SADB_EXT_SPIRANGE;
     sspi.sadb_spirange_len = sizeof(sspi) / 8;
     if (spi) {
	  sspi.sadb_spirange_min = spi;
	  sspi.sadb_spirange_max = spi;
     } else {
	  sspi.sadb_spirange_min = 0x100;
	  sspi.sadb_spirange_max = -1;
     }
     iov[cnt].iov_base = &sspi;
     iov[cnt++].iov_len = sizeof(sspi);
     smsg.sadb_msg_len += sspi.sadb_spirange_len;
     
     /* get back SADB_EXT_SA */

     if (!KERNEL_XF_SET(smsg.sadb_msg_len*8)) {
	  log_error(1, "kernel_xf_set() in kernel_reserve_single_spi()");
	  return 0;
     }

     sres = (struct sadb_msg *)buffer;
     ssa = (struct sadb_sa *)(sres + 1);
     if (ssa->sadb_sa_exttype != SADB_EXT_SA) {
	  log_error(0, "SADB_GETSPI did not return a SADB_EXT_SA struct: %d",
		    ssa->sadb_sa_exttype);
	  return 0;
     }

     return ntohl(ssa->sadb_sa_spi);
}

int
kernel_ah(attrib_t *ob, struct spiob *SPI, u_int8_t *secrets, int hmac)
{
     struct sadb_msg sa;
     struct sadb_address sad1;
     struct sadb_address sad2;
     struct sadb_sa sr;
     struct sadb_lifetime sl;
     struct sadb_key sk;
     struct sockaddr_in src;
     struct sockaddr_in dst;
     struct iovec iov[20];
     int len, cnt = 0;
     transform *xf = kernel_get_transform(ob->id);
     time_t now = time(NULL);

     if (xf == NULL || !(xf->flags & XF_AUTH)) {
	  log_error(0, "%d is not an auth transform in kernel_ah()", ob->id);
	  return -1;
     }

     bzero(&sa, sizeof(sa));
     bzero(&sad1, sizeof(sad1));
     bzero(&sad2, sizeof(sad2));
     bzero(&sr, sizeof(sr));
     bzero(&sk, sizeof(sk));
     bzero(&sl, sizeof(sl));
     bzero(&src, sizeof(src));
     bzero(&dst, sizeof(dst));

     sa.sadb_msg_len = sizeof(sa) / 8;
     sa.sadb_msg_version = PF_KEY_V2;
     sa.sadb_msg_type = SPI->flags & SPI_OWNER ? 
	  SADB_UPDATE : SADB_ADD;
     sa.sadb_msg_satype = !hmac ?
	  SADB_SATYPE_X_AH_OLD : SADB_SATYPE_AH;
     sa.sadb_msg_seq = pfkey_seq++;
     sa.sadb_msg_pid = pfkey_pid;
     iov[cnt].iov_base = &sa;
     len = iov[cnt++].iov_len = sizeof(sa);

     /* Source Address */
     sad1.sadb_address_len = 1 + sizeof(struct sockaddr_in) / 8;
     sad1.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
     src.sin_family = AF_INET;
     src.sin_len = sizeof(struct sockaddr_in);
     src.sin_addr.s_addr = inet_addr(SPI->flags & SPI_OWNER ? 
				     SPI->address : SPI->local_address);
     sa.sadb_msg_len += sad1.sadb_address_len;

     iov[cnt].iov_base = &sad1;
     len += iov[cnt++].iov_len = sizeof(sad1);
     iov[cnt].iov_base = &src;
     len += iov[cnt++].iov_len = sizeof(struct sockaddr);

     /* Destination Address */
     sad2.sadb_address_len = 1 + sizeof(struct sockaddr_in) / 8;
     sad2.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
     dst.sin_family = AF_INET;
     dst.sin_len = sizeof(struct sockaddr_in);
     dst.sin_addr.s_addr = inet_addr(SPI->flags & SPI_OWNER ?
				     SPI->local_address : SPI->address);
     sa.sadb_msg_len += sad2.sadb_address_len;

     iov[cnt].iov_base = &sad2;
     len += iov[cnt++].iov_len = sizeof(sad2);
     iov[cnt].iov_base = &dst;
     len += iov[cnt++].iov_len = sizeof(struct sockaddr);

     sr.sadb_sa_len = sizeof(sr) / 8;
     sr.sadb_sa_exttype = SADB_EXT_SA;
     sr.sadb_sa_spi = htonl(SPITOINT(SPI->SPI));
     sr.sadb_sa_replay = !hmac ? 0 : 32;
     sr.sadb_sa_state = SADB_SASTATE_MATURE;
     sr.sadb_sa_auth = xf->kernel_id;
     sr.sadb_sa_encrypt = 0;
     if (SPI->flags & SPI_TUNNEL)
	  sr.sadb_sa_flags |= SADB_SAFLAGS_X_TUNNEL;
     sa.sadb_msg_len += sr.sadb_sa_len;

     iov[cnt].iov_base = &sr;
     len += iov[cnt++].iov_len = sizeof(sr);

     sl.sadb_lifetime_len = sizeof(sl) / 8;
     sl.sadb_lifetime_exttype = SADB_EXT_LIFETIME_HARD;
     sl.sadb_lifetime_allocations = 10;   /* 10 flows */
     sl.sadb_lifetime_bytes = 1000000000;   /* lots of bytes */
     sl.sadb_lifetime_addtime = SPI->lifetime + 60 - now;
     sl.sadb_lifetime_usetime = SPI->lifetime - now; /* first use */
     sa.sadb_msg_len += sl.sadb_lifetime_len;

     iov[cnt].iov_base = &sl;
     len += iov[cnt++].iov_len = sizeof(sl);

     sk.sadb_key_len = (sizeof(sk) + ob->klen + 7) / 8;
     sk.sadb_key_exttype = SADB_EXT_KEY_AUTH;
     sk.sadb_key_bits = ob->klen * 8;
     sa.sadb_msg_len += sk.sadb_key_len;

     iov[cnt].iov_base = &sk;
     len += iov[cnt++].iov_len = sizeof(sk);
     iov[cnt].iov_base = secrets;
     len += iov[cnt++].iov_len = ((ob->klen + 7) / 8) * 8;

     kernel_debug(("kernel_ah: %08x. %s-Mode\n", 
		   ntohl(sr.sadb_sa_spi),
		   SPI->flags & SPI_TUNNEL ? "Tunnel" : "Transport"));

     if (!KERNEL_XF_SET(len)) {
	  log_error(1, "kernel_xf_set() in kernel_ah()");
	  return -1;
     }
     return ob->klen;
}

int
kernel_esp(attrib_t *ob, attrib_t *ob2, struct spiob *SPI, u_int8_t *secrets)
{
     struct sadb_msg sa;
     struct sadb_address sad1;
     struct sadb_address sad2;
     struct sadb_sa sr;
     struct sadb_lifetime sl;
     struct sadb_key sk1;
     struct sadb_key sk2;
     struct sockaddr_in src;
     struct sockaddr_in dst;
     struct iovec iov[20];
     attrib_t *attenc, *attauth = NULL;
     u_int8_t *sec1, *sec2 = NULL;
     transform *xf_enc, *xf_auth = NULL;
     int cnt = 0;
     time_t now = time(NULL);

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

     bzero(&sa, sizeof(sa));
     bzero(&sad1, sizeof(sad1));
     bzero(&sad2, sizeof(sad2));
     bzero(&sr, sizeof(sr));
     bzero(&sk1, sizeof(sk1));
     bzero(&sk2, sizeof(sk2));
     bzero(&sl, sizeof(sl));
     bzero(&src, sizeof(src));
     bzero(&dst, sizeof(dst));

     sa.sadb_msg_len = sizeof(sa) / 8;
     sa.sadb_msg_version = PF_KEY_V2;
     sa.sadb_msg_type = SPI->flags & SPI_OWNER ?
	  SADB_UPDATE : SADB_ADD;
     sa.sadb_msg_satype = xf_enc->flags & ESP_OLD ?
	  SADB_SATYPE_X_ESP_OLD : SADB_SATYPE_ESP;
     sa.sadb_msg_seq = pfkey_seq++;
     sa.sadb_msg_pid = pfkey_pid;
     iov[cnt].iov_base = &sa;
     iov[cnt++].iov_len = sizeof(sa);

     sr.sadb_sa_len = sizeof(sr) / 8;
     sr.sadb_sa_exttype = SADB_EXT_SA;
     sr.sadb_sa_spi = htonl(SPITOINT(SPI->SPI));
     sr.sadb_sa_replay = xf_enc->flags & ESP_OLD ? 0 : 32;
     sr.sadb_sa_state = SADB_SASTATE_MATURE;
     sr.sadb_sa_auth = attauth ? xf_auth->kernel_id : 0;
     sr.sadb_sa_encrypt = xf_enc->kernel_id;
     if (xf_enc->flags & ESP_OLD)
	  sr.sadb_sa_flags |= SADB_SAFLAGS_X_HALFIV;
     if (SPI->flags & SPI_TUNNEL)
	  sr.sadb_sa_flags |= SADB_SAFLAGS_X_TUNNEL;
     sa.sadb_msg_len += sr.sadb_sa_len;

     iov[cnt].iov_base = &sr;
     iov[cnt++].iov_len = sizeof(sr);

     /* Source Address */
     sad1.sadb_address_len = 1 + sizeof(struct sockaddr_in) / 8;
     sad1.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
     src.sin_family = AF_INET;
     src.sin_len = sizeof(struct sockaddr_in);
     src.sin_addr.s_addr = inet_addr(SPI->flags & SPI_OWNER ? 
				     SPI->address : SPI->local_address);
     sa.sadb_msg_len += sad1.sadb_address_len;

     iov[cnt].iov_base = &sad1;
     iov[cnt++].iov_len = sizeof(sad1);
     iov[cnt].iov_base = &src;
     iov[cnt++].iov_len = sizeof(struct sockaddr);

     /* Destination Address */
     sad2.sadb_address_len = 1 + sizeof(struct sockaddr_in) / 8;
     sad2.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
     dst.sin_family = AF_INET;
     dst.sin_len = sizeof(struct sockaddr_in);
     dst.sin_addr.s_addr = inet_addr(SPI->flags & SPI_OWNER ?
				     SPI->local_address : SPI->address);
     sa.sadb_msg_len += sad2.sadb_address_len;

     iov[cnt].iov_base = &sad2;
     iov[cnt++].iov_len = sizeof(sad2);
     iov[cnt].iov_base = &dst;
     iov[cnt++].iov_len = sizeof(struct sockaddr);

     sl.sadb_lifetime_len = sizeof(sl) / 8;
     sl.sadb_lifetime_exttype = SADB_EXT_LIFETIME_HARD;
     sl.sadb_lifetime_allocations = 10;   /* 10 flows */
     sl.sadb_lifetime_bytes = 1000000000;   /* lots of bytes */
     sl.sadb_lifetime_addtime = SPI->lifetime + 60 - now;
     sl.sadb_lifetime_usetime = SPI->lifetime - now; /* first use */
     sa.sadb_msg_len += sl.sadb_lifetime_len;

     iov[cnt].iov_base = &sl;
     iov[cnt++].iov_len = sizeof(sl);

     sk1.sadb_key_len = (sizeof(sk1) + attenc->klen + 7) / 8;
     sk1.sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
     sk1.sadb_key_bits = attenc->klen * 8;
     sa.sadb_msg_len += sk1.sadb_key_len;

     iov[cnt].iov_base = &sk1;
     iov[cnt++].iov_len = sizeof(sk1);
     iov[cnt].iov_base = sec1;
     iov[cnt++].iov_len = ((attenc->klen + 7) / 8) * 8;

     if (attauth != NULL) {
	  sk2.sadb_key_len = (sizeof(sk2) + attauth->klen + 7) / 8;
	  sk2.sadb_key_exttype = SADB_EXT_KEY_AUTH;
	  sk2.sadb_key_bits = attauth->klen * 8;
	  sa.sadb_msg_len += sk2.sadb_key_len;

	  iov[cnt].iov_base = &sk2;
	  iov[cnt++].iov_len = sizeof(sk2);
	  iov[cnt].iov_base = sec2;
	  iov[cnt++].iov_len = ((attauth->klen + 7) / 8) * 8;
     }

     kernel_debug(("kernel_esp: %08x. %s-Mode\n", 
		   ntohl(sr.sadb_sa_spi),
		   SPI->flags & SPI_TUNNEL ? "Tunnel" : "Transport"));

     if (!KERNEL_XF_SET(sa.sadb_msg_len * 8)) {
	  log_error(1, "kernel_xf_set() in kernel_esp()");
	  return -1;
     }
     
     return attenc->klen + (attauth ? attauth->klen : 0);
}

/* Group an ESP SPI with an AH SPI */

int
kernel_group_spi(char *address, u_int8_t *spi)
{
     struct sadb_msg smsg;
     struct sadb_sa sa, sa2;
     struct sadb_address sad1, sad2;
     struct sadb_protocol sproto;
     union sockaddr_union dst1, dst2;
     struct iovec iov[8];
     int cnt = 0;
     u_int32_t SPI;

     SPI = SPITOINT(spi);

     bzero(&smsg, sizeof(smsg));
     bzero(&sa, sizeof(sa));
     bzero(&sa2, sizeof(sa2));
     bzero(&sad1, sizeof(sad1));
     bzero(&sad2, sizeof(sad2));
     bzero(&dst1, sizeof(dst1));
     bzero(&dst2, sizeof(dst2));

     smsg.sadb_msg_version = PF_KEY_V2;
     smsg.sadb_msg_seq = pfkey_seq++;
     smsg.sadb_msg_pid = pfkey_pid;
     smsg.sadb_msg_type = SADB_X_GRPSPIS;
     smsg.sadb_msg_satype = SADB_SATYPE_ESP;
     smsg.sadb_msg_len = sizeof(smsg) / 8;

     iov[cnt].iov_base = &smsg;
     iov[cnt++].iov_len = sizeof(smsg);

     sa.sadb_sa_len = sizeof(sa) / 8;
     sa.sadb_sa_exttype = SADB_EXT_SA;
     sa.sadb_sa_spi = htonl(SPI);
     sa.sadb_sa_state = SADB_SASTATE_MATURE;
     smsg.sadb_msg_len += sa.sadb_sa_len;

     iov[cnt].iov_base = &sa;
     iov[cnt++].iov_len = sizeof(sa);

     sa2.sadb_sa_len = sizeof(sa2) / 8;
     sa2.sadb_sa_exttype = SADB_EXT_X_SA2;
     sa2.sadb_sa_spi = htonl(SPI);
     sa2.sadb_sa_state = SADB_SASTATE_MATURE;
     smsg.sadb_msg_len += sa2.sadb_sa_len;

     iov[cnt].iov_base = &sa2;
     iov[cnt++].iov_len = sizeof(sa2);

     sad1.sadb_address_len = (sizeof(sad1) + sizeof(struct sockaddr_in)) / 8;
     sad1.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
     iov[cnt].iov_base = &sad1;
     iov[cnt++].iov_len = sizeof(sad1);
     dst1.sin.sin_family = AF_INET;
     dst1.sin.sin_len = sizeof(struct sockaddr_in);
     dst1.sin.sin_addr.s_addr = inet_addr(address);
     smsg.sadb_msg_len += sad1.sadb_address_len;
     iov[cnt].iov_base = &dst1;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     sad2.sadb_address_len = (sizeof(sad2) + sizeof(struct sockaddr_in)) / 8;
     sad2.sadb_address_exttype = SADB_EXT_X_DST2;
     iov[cnt].iov_base = &sad2;
     iov[cnt++].iov_len = sizeof(sad2);
     dst2.sin.sin_family = AF_INET;
     dst2.sin.sin_len = sizeof(struct sockaddr_in);
     dst2.sin.sin_addr.s_addr = inet_addr(address);
     smsg.sadb_msg_len += sad2.sadb_address_len;
     iov[cnt].iov_base = &dst2;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     sproto.sadb_protocol_len = sizeof(sproto) / 8;
     sproto.sadb_protocol_exttype = SADB_EXT_X_PROTOCOL;
     sproto.sadb_protocol_proto = SADB_SATYPE_AH;
     smsg.sadb_msg_len += sproto.sadb_protocol_len;
     iov[cnt].iov_base = &sproto;
     iov[cnt++].iov_len = sizeof(sproto);

     kernel_debug(("kernel_group_spi: %s, %08x\n", address, SPI));

     if (!KERNEL_XF_SET(smsg.sadb_msg_len*8)) {
	  log_error(1, "kernel_xf_set() in kernel_group_spi()");
	  return -1;
     }

     return 1;
}

int
kernel_bind_spis(struct spiob *spi1, struct spiob *spi2)
{
     struct sadb_msg smsg;
     struct sadb_sa sa, sa2;
     struct sadb_address sad1, sad2;
     struct sadb_protocol sproto;
     union sockaddr_union dst1, dst2;
     struct iovec iov[8];
     int cnt = 0;
     u_int32_t inspi = SPITOINT(spi1->SPI);
     u_int32_t outspi = SPITOINT(spi2->SPI);

     bzero(&smsg, sizeof(smsg));
     bzero(&sa, sizeof(sa));
     bzero(&sa2, sizeof(sa2));
     bzero(&sad1, sizeof(sad1));
     bzero(&sad2, sizeof(sad2));
     bzero(&dst1, sizeof(dst1));
     bzero(&dst2, sizeof(dst2));

     smsg.sadb_msg_version = PF_KEY_V2;
     smsg.sadb_msg_seq = pfkey_seq++;
     smsg.sadb_msg_pid = pfkey_pid;
     smsg.sadb_msg_type = SADB_X_BINDSA;
     smsg.sadb_msg_satype = spi1->flags & SPI_ESP ?
	  SADB_SATYPE_ESP : SADB_SATYPE_AH;
     smsg.sadb_msg_len = sizeof(smsg) / 8;

     iov[cnt].iov_base = &smsg;
     iov[cnt++].iov_len = sizeof(smsg);

     sa.sadb_sa_len = sizeof(sa) / 8;
     sa.sadb_sa_exttype = SADB_EXT_SA;
     sa.sadb_sa_spi = htonl(inspi);
     sa.sadb_sa_state = SADB_SASTATE_MATURE;
     smsg.sadb_msg_len += sa.sadb_sa_len;

     iov[cnt].iov_base = &sa;
     iov[cnt++].iov_len = sizeof(sa);

     sa2.sadb_sa_len = sizeof(sa2) / 8;
     sa2.sadb_sa_exttype = SADB_EXT_X_SA2;
     sa2.sadb_sa_spi = htonl(outspi);
     sa2.sadb_sa_state = SADB_SASTATE_MATURE;
     smsg.sadb_msg_len += sa2.sadb_sa_len;

     iov[cnt].iov_base = &sa2;
     iov[cnt++].iov_len = sizeof(sa2);

     sad1.sadb_address_len = (sizeof(sad1) + sizeof(struct sockaddr_in)) / 8;
     sad1.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
     iov[cnt].iov_base = &sad1;
     iov[cnt++].iov_len = sizeof(sad1);
     dst1.sin.sin_family = AF_INET;
     dst1.sin.sin_len = sizeof(struct sockaddr_in);
     dst1.sin.sin_addr.s_addr = inet_addr(spi1->local_address);
     smsg.sadb_msg_len += sad1.sadb_address_len;
     iov[cnt].iov_base = &dst1;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     sad2.sadb_address_len = (sizeof(sad2) + sizeof(struct sockaddr_in)) / 8;
     sad2.sadb_address_exttype = SADB_EXT_X_DST2;
     iov[cnt].iov_base = &sad2;
     iov[cnt++].iov_len = sizeof(sad2);
     dst2.sin.sin_family = AF_INET;
     dst2.sin.sin_len = sizeof(struct sockaddr_in);
     dst2.sin.sin_addr.s_addr = inet_addr(spi2->address);
     smsg.sadb_msg_len += sad2.sadb_address_len;
     iov[cnt].iov_base = &dst2;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     sproto.sadb_protocol_len = sizeof(sproto) / 8;
     sproto.sadb_protocol_exttype = SADB_EXT_X_PROTOCOL;
     sproto.sadb_protocol_proto = spi2->flags & SPI_ESP ?
	  SADB_SATYPE_ESP : SADB_SATYPE_AH;
     smsg.sadb_msg_len += sproto.sadb_protocol_len;
     iov[cnt].iov_base = &sproto;
     iov[cnt++].iov_len = sizeof(sproto);

     kernel_debug(("kernel_bind_spi: <%s, %08x> -> <%s, %08x>\n",
		   spi1->local_address, inspi, spi2->address, outspi));

     if (!KERNEL_XF_SET(smsg.sadb_msg_len*8)) {
	  log_error(1, "kernel_xf_set() in kernel_bind_spi()");
	  return -1;
     }

     return 1;
}

int
kernel_enable_spi(in_addr_t isrc, in_addr_t ismask, 
		  in_addr_t idst, in_addr_t idmask, 
		  char *address, u_int8_t *spi, int proto, int flags)
{
     struct sadb_msg smsg;
     struct sadb_sa sa;
     struct sadb_address sad, sad1, sad2, sad3, sad4;
     union sockaddr_union dst, osrc, osmask, odst, odmask;
     struct iovec iov[12];
     u_int32_t SPI;
     int cnt = 0;
     SPI = SPITOINT(spi);

     bzero(&smsg, sizeof(smsg));
     bzero(&sa, sizeof(sa));
     bzero(&sad, sizeof(sad));
     bzero(&sad1, sizeof(sad1));
     bzero(&sad2, sizeof(sad2));
     bzero(&sad3, sizeof(sad3));
     bzero(&sad4, sizeof(sad4));
     bzero(&dst, sizeof(dst));
     bzero(&osrc, sizeof(osrc));
     bzero(&osmask, sizeof(osmask));
     bzero(&odst, sizeof(odst));
     bzero(&odmask, sizeof(odmask));
     
     smsg.sadb_msg_len = sizeof(smsg) / 8;
     smsg.sadb_msg_version = PF_KEY_V2;
     smsg.sadb_msg_seq = pfkey_seq++;
     smsg.sadb_msg_pid = pfkey_pid;
     smsg.sadb_msg_type = SADB_X_ADDFLOW;
     smsg.sadb_msg_satype = proto == IPPROTO_ESP ?
	  SADB_SATYPE_ESP : SADB_SATYPE_AH;
     iov[cnt].iov_base = &smsg;
     iov[cnt++].iov_len = sizeof(smsg);

     sa.sadb_sa_len = sizeof(sa) / 8;
     sa.sadb_sa_exttype = SADB_EXT_SA;
     sa.sadb_sa_state = SADB_SASTATE_MATURE;
     sa.sadb_sa_spi = htonl(SPI);
     sa.sadb_sa_flags = flags;
     smsg.sadb_msg_len += sa.sadb_sa_len;
     iov[cnt].iov_base = &sa;
     iov[cnt++].iov_len = sizeof(sa);

     sad.sadb_address_len = (sizeof(sad) + sizeof(struct sockaddr_in)) / 8;
     sad.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
     smsg.sadb_msg_len += sad.sadb_address_len;
     iov[cnt].iov_base = &sad;
     iov[cnt++].iov_len = sizeof(sad);

     dst.sin.sin_family = AF_INET;
     dst.sin.sin_len = sizeof(struct sockaddr_in);
     dst.sin.sin_addr.s_addr = inet_addr(address);
     iov[cnt].iov_base = &dst;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     sad1.sadb_address_exttype = SADB_EXT_X_SRC_FLOW;
     sad2.sadb_address_exttype = SADB_EXT_X_SRC_MASK;
     sad3.sadb_address_exttype = SADB_EXT_X_DST_FLOW;
     sad4.sadb_address_exttype = SADB_EXT_X_DST_MASK;
     
     sad1.sadb_address_len = (sizeof(sad1) + sizeof(struct sockaddr_in)) / 8;
     sad2.sadb_address_len = (sizeof(sad2) + sizeof(struct sockaddr_in)) / 8;
     sad3.sadb_address_len = (sizeof(sad3) + sizeof(struct sockaddr_in)) / 8;
     sad4.sadb_address_len = (sizeof(sad4) + sizeof(struct sockaddr_in)) / 8;
     
     osrc.sin.sin_family = odst.sin.sin_family = AF_INET;
     osmask.sin.sin_family = odmask.sin.sin_family = AF_INET;
     osrc.sin.sin_len = odst.sin.sin_len = sizeof(struct sockaddr_in);
     osmask.sin.sin_len = sizeof(struct sockaddr_in);
     odmask.sin.sin_len = sizeof(struct sockaddr_in);
	    
     osrc.sin.sin_addr.s_addr = isrc;
     osmask.sin.sin_addr.s_addr = ismask;
     odst.sin.sin_addr.s_addr = idst;
     odmask.sin.sin_addr.s_addr = idmask;
     smsg.sadb_msg_len += sad1.sadb_address_len * 4;

     iov[cnt].iov_base = &sad1;
     iov[cnt++].iov_len = sizeof(sad1);
     iov[cnt].iov_base = &osrc;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     iov[cnt].iov_base = &sad2;
     iov[cnt++].iov_len = sizeof(sad2);
     iov[cnt].iov_base = &osmask;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     iov[cnt].iov_base = &sad3;
     iov[cnt++].iov_len = sizeof(sad3);
     iov[cnt].iov_base = &odst;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     iov[cnt].iov_base = &sad4;
     iov[cnt++].iov_len = sizeof(sad4);
     iov[cnt].iov_base = &odmask;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     kernel_debug(("kernel_enable_spi: %08x\n", SPI));

     if (!KERNEL_XF_SET(smsg.sadb_msg_len*8)) {
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
     struct sadb_msg smsg;
     struct sadb_sa sa;
     struct sadb_address sad1, sad2, sad3, sad4;
     union sockaddr_union osrc, osmask, odst, odmask;
     struct iovec iov[12];
     u_int32_t SPI;
     int cnt = 0;
     SPI = SPITOINT(spi);

     bzero(&smsg, sizeof(smsg));
     bzero(&sa, sizeof(sa));
     bzero(&sad1, sizeof(sad1));
     bzero(&sad2, sizeof(sad2));
     bzero(&sad3, sizeof(sad3));
     bzero(&sad4, sizeof(sad4));
     bzero(&osrc, sizeof(osrc));
     bzero(&osmask, sizeof(osmask));
     bzero(&odst, sizeof(odst));
     bzero(&odmask, sizeof(odmask));
     
     smsg.sadb_msg_len = sizeof(smsg) / 8;
     smsg.sadb_msg_version = PF_KEY_V2;
     smsg.sadb_msg_seq = pfkey_seq++;
     smsg.sadb_msg_pid = pfkey_pid;
     smsg.sadb_msg_type = SADB_X_DELFLOW;
     smsg.sadb_msg_satype = proto == IPPROTO_ESP ?
	  SADB_SATYPE_ESP : SADB_SATYPE_AH;
     iov[cnt].iov_base = &smsg;
     iov[cnt++].iov_len = sizeof(smsg);

     sa.sadb_sa_len = sizeof(sa) / 8;
     sa.sadb_sa_exttype = SADB_EXT_SA;
     sa.sadb_sa_state = SADB_SASTATE_MATURE;
     sa.sadb_sa_spi = htonl(SPI);
     sa.sadb_sa_flags = flags;
     smsg.sadb_msg_len += sa.sadb_sa_len;
     iov[cnt].iov_base = &sa;
     iov[cnt++].iov_len = sizeof(sa);

     sad1.sadb_address_exttype = SADB_EXT_X_SRC_FLOW;
     sad2.sadb_address_exttype = SADB_EXT_X_SRC_MASK;
     sad3.sadb_address_exttype = SADB_EXT_X_DST_FLOW;
     sad4.sadb_address_exttype = SADB_EXT_X_DST_MASK;
     
     sad1.sadb_address_len = (sizeof(sad1) + sizeof(struct sockaddr_in)) / 8;
     sad2.sadb_address_len = (sizeof(sad2) + sizeof(struct sockaddr_in)) / 8;
     sad3.sadb_address_len = (sizeof(sad3) + sizeof(struct sockaddr_in)) / 8;
     sad4.sadb_address_len = (sizeof(sad4) + sizeof(struct sockaddr_in)) / 8;
     
     osrc.sin.sin_family = odst.sin.sin_family = AF_INET;
     osmask.sin.sin_family = odmask.sin.sin_family = AF_INET;
     osrc.sin.sin_len = odst.sin.sin_len = sizeof(struct sockaddr_in);
     osmask.sin.sin_len = sizeof(struct sockaddr_in);
     odmask.sin.sin_len = sizeof(struct sockaddr_in);
	    
     osrc.sin.sin_addr.s_addr = isrc;
     osmask.sin.sin_addr.s_addr = ismask;
     odst.sin.sin_addr.s_addr = idst;
     odmask.sin.sin_addr.s_addr = idmask;
     smsg.sadb_msg_len += sad1.sadb_address_len * 4;

     iov[cnt].iov_base = &sad1;
     iov[cnt++].iov_len = sizeof(sad1);
     iov[cnt].iov_base = &osrc;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     iov[cnt].iov_base = &sad2;
     iov[cnt++].iov_len = sizeof(sad2);
     iov[cnt].iov_base = &osmask;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     iov[cnt].iov_base = &sad3;
     iov[cnt++].iov_len = sizeof(sad3);
     iov[cnt].iov_base = &odst;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     iov[cnt].iov_base = &sad4;
     iov[cnt++].iov_len = sizeof(sad4);
     iov[cnt].iov_base = &odmask;
     iov[cnt++].iov_len = sizeof(struct sockaddr_in);

     kernel_debug(("kernel_disable_spi: %08x\n", SPI));

     if (!KERNEL_XF_SET(smsg.sadb_msg_len*8)) {
	  log_error(1, "kernel_xf_set() in kernel_disable_spi()");
	  return -1;
     }

     return 1;
}

/*
 * Remove a single SPI from the kernel database.
 */

int
kernel_delete_spi(char *address, u_int32_t spi, int proto)
{
     struct sadb_msg sa; 
     struct sadb_sa sr; 
     struct sadb_address sad1; 
     struct sadb_address sad2; 
     union sockaddr_union src, dst;
     struct iovec iov[10]; 
     int cnt = 0; 
 
     bzero(&sa, sizeof(sa)); 
     bzero(&sad1, sizeof(sad1)); 
     bzero(&sad2, sizeof(sad2)); 
     bzero(&sr, sizeof(sr)); 
     bzero(&src, sizeof(src)); 
     bzero(&dst, sizeof(dst)); 
   
     sa.sadb_msg_version = PF_KEY_V2; 
     sa.sadb_msg_type = SADB_DELETE; 
     sa.sadb_msg_satype = proto == IPPROTO_ESP ?
	  SADB_SATYPE_ESP : SADB_SATYPE_AH; 
     sa.sadb_msg_seq = pfkey_seq++; 
     sa.sadb_msg_pid = pfkey_pid; 
 
     /* Source Address */
     sad1.sadb_address_len = 1 + sizeof(struct sockaddr_in) / 8; 
     sad1.sadb_address_exttype = SADB_EXT_ADDRESS_SRC; 

     src.sin.sin_family = AF_INET; 
     src.sin.sin_len = sizeof(struct sockaddr_in); 
  
     /* Destination Address */
     sad2.sadb_address_len = 1 + sizeof(struct sockaddr_in) / 8; 
     sad2.sadb_address_exttype = SADB_EXT_ADDRESS_DST; 

     dst.sin.sin_family = AF_INET; 
     dst.sin.sin_len = sizeof(struct sockaddr_in); 
     dst.sin.sin_addr.s_addr = inet_addr(address); 
 
     sr.sadb_sa_exttype = SADB_EXT_SA; 
     sr.sadb_sa_spi = htonl(spi);
     sr.sadb_sa_len = sizeof(sr) / 8; 
 
     sa.sadb_msg_len = 2 + sr.sadb_sa_len + sad2.sadb_address_len + 
	  sad1.sadb_address_len; 
    
     iov[cnt].iov_base = &sa; 
     iov[cnt++].iov_len = sizeof(sa); 
     iov[cnt].iov_base = &sad1; 
     iov[cnt++].iov_len = sizeof(sad1); 
     iov[cnt].iov_base = &src; 
     iov[cnt++].iov_len = sizeof(struct sockaddr); 
     iov[cnt].iov_base = &sad2; 
     iov[cnt++].iov_len = sizeof(sad2); 
     iov[cnt].iov_base = &dst;
     iov[cnt++].iov_len = sizeof(struct sockaddr); 
     iov[cnt].iov_base = &sr; 
     iov[cnt++].iov_len = sizeof(sr); 
    

     kernel_debug(("kernel_delete_spi: %08x\n", spi));

     if (!KERNEL_XF_SET(sa.sadb_msg_len * 8)) {
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
     struct spiob *spi2;
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
	       log_error(0, "No encryption attribute in ESP section for SA(%08x, %s->%s) in kernel_insert()", SPITOINT(SPI->SPI), SPI->local_address, SPI->address);
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
	       log_error(0, "No authentication attribute in AH section for SA(%08x, %s->%s) in kernel_insert()", SPITOINT(SPI->SPI), SPI->local_address, SPI->address);
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

     if (esp != NULL) {
	  proto = IPPROTO_ESP;
	  SPI->flags |= SPI_ESP;
     } else {
	  proto = IPPROTO_AH;
	  SPI->flags &= ~SPI_ESP;
     }

     /* Group the SPIs for User */
     if (!(SPI->flags & SPI_OWNER) && ah != NULL && esp != NULL) {
	  if (kernel_group_spi(SPI->address, spi) == -1)
	       log_error(0, "kernel_group_spi() in kernel_insert_spi()");
     }
     
     if (!(SPI->flags & SPI_OWNER)) {
	  if (!(SPI->flags & SPI_NOTIFY) || vpn_mode) {
	       if (kernel_enable_spi(SPI->isrc, SPI->ismask,
				     SPI->idst, SPI->idmask,
				     SPI->address, spi, proto, 
				     SADB_SAFLAGS_X_REPLACEFLOW | SADB_SAFLAGS_X_LOCALFLOW |
				     (vpn_mode ? /*ENABLE_FLAG_MODIFY*/ : 0)) == -1)
		    log_error(0, "kernel_enable_spi() in kernel_insert_spi()");
	  } else {
	       /* 
		* Inform the kernel that we obtained the requested SA
		*/
		    kernel_notify_result(st, SPI, proto);
	  }
     }
	  
     /* Is this what people call perfect forward security ? */
     bzero(SPI->sessionkey, SPI->sessionkeysize);
     free(SPI->sessionkey);
     SPI->sessionkey = NULL; SPI->sessionkeysize = 0;

     /* Bind the pair of SPI in the state object */
     if (SPI->flags & SPI_OWNER)
	  spi2 = spi_find(SPI->address, st->uSPI);
     else
	  spi2 = spi_find(SPI->local_address, st->oSPI);

     if (!spi2) {
	  log_error(0, "kernel_insert_spi(): can not find second SPI");
	  return 0;
     }

     if (SPI->flags & SPI_OWNER)
	  kernel_bind_spis(SPI, spi2);
     else
	  kernel_bind_spis(spi2, SPI);

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
	  int flag = (vpn_mode ? /*ENABLE_FLAG_MODIFY*/ : 0) | SADB_SAFLAGS_X_LOCALFLOW;
	  if (!(ospi->flags & SPI_OWNER) && 
	      kernel_disable_spi(ospi->isrc, ospi->ismask,
				 ospi->idst, ospi->idmask,
				 ospi->address, ospi->SPI, 
				 IPPROTO_ESP, flag) == -1)
	       log_error(0, "kernel_disable_spi() in kernel_unlink_spi()");

	  if (kernel_delete_spi(p, SPITOINT(ospi->SPI), IPPROTO_ESP) == -1)
	       log_error(0, "kernel_delete_spi() in kernel_unlink_spi()");
     }
	  
     if (ah != NULL) {
	  if (esp == NULL) {
	       int flag = (vpn_mode ? /*ENABLE_FLAG_MODIFY*/ : 0) | 
					SADB_SAFLAGS_X_LOCALFLOW;
	       if (!(ospi->flags & SPI_OWNER) &&
		   kernel_disable_spi(ospi->isrc, ospi->ismask,
				      ospi->idst, ospi->idmask,
				      ospi->address, ospi->SPI, 
				      IPPROTO_AH, flag) == -1)
		    log_error(0, "kernel_disable_spi() in kernel_unlink_spi()");
	  }
	  
	  if (kernel_delete_spi(p, SPITOINT(ospi->SPI), IPPROTO_AH) == -1)
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
     struct sadb_msg *sres = (struct sadb_msg *)buffer;

     if (!kernel_xf_read(regsd, buffer, BUFFER_SIZE, 0))
	  return;

#ifdef DEBUG
     kernel_debug(("Got PFKEYV2 message: type %d\n", sres->sadb_msg_type));
#endif

     switch (sres->sadb_msg_type) {
     case SADB_EXPIRE:
	  log_error(0, "PFKEYV2 SA Expiration - not yet supported.\n");
	  return;
     case SADB_ACQUIRE:
#ifdef DEBUG
	  kernel_debug(("Got Notify SA Request (SADB_ACQUIRE)\n"));
#endif
	  kernel_request_sa(sres);
	  break;
     default:
	  /* discard silently */
	  return; 
	  }
}

/*
 * Tries to establish a new SA according to the information in a 
 * REQUEST_SA notify message received from the kernel.
 */

int
kernel_request_sa(void *em /*struct encap_msghdr *em*/) 
{
/*     struct stateob *st;
     time_t tm;
     char *address = inet_ntoa(em->em_not_dst);

     /#* Try to find an already established exchange which is still valid *#/
     st = state_find(address);

     tm = time(NULL);
     while (st != NULL && (st->lifetime <= tm || st->phase >= SPI_UPDATE))
	  st = state_find_next(st, address);

     if (st == NULL) {
	  /#* No established exchange found, start a new one *#/
	  if ((st = state_new()) == NULL) {
	       log_error(0, "state_new() failed in kernel_request_sa() for remote ip %s",
			 address);
	       return (-1);
	  }
	  /#* Set up the state information *#/
	  strncpy(st->address, address, sizeof(st->address)-1);
	  st->port = global_port;
	  st->sport = em->em_not_sport;
	  st->dport = em->em_not_dport;
	  st->protocol = em->em_not_protocol;

	  /#*
	   * For states which were created by kernel notifies we wont
	   * set up routes since other keying daemons might habe beaten
	   * us in establishing SAs. The kernel has to decide which SA
	   * will actually be routed.
	   *#/
	  st->flags = IPSEC_NOTIFY;
	  if (em->em_not_satype & NOTIFY_SATYPE_CONF)
	       st->flags |= IPSEC_OPT_ENC;
	  if (em->em_not_satype & NOTIFY_SATYPE_AUTH)
	       st->flags |= IPSEC_OPT_AUTH;
	  /#* XXX - handling of tunnel requests missing *#/
	  if (start_exchange(global_socket, st, st->address, st->port) == -1) {
	       log_error(0, "start_exchange() in kernel_request_sa() - informing kernel of failure");
	       /#* Inform kernel of our failure *#/
	       kernel_notify_result(st, NULL, 0);
	       state_value_reset(st);
	       free(st);
	       return (-1);
	  } else
	       state_insert(st);
     } else {
	  /#* 
	   * We need different attributes for this exchange, send
	   * an SPI_NEEDED message.
	   *#/
     }
*/
}

/*
 * Report the established SA or either our failure to create an SA
 * to the kernel.
 * Passing a SPI of NULL means failure.
 */

void
kernel_notify_result(struct stateob *st, struct spiob *spi, int proto)
{

     /*     struct encap_msghdr em;

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
     log_error(1, "kernel_xf_set() in kernel_notify_result()"); */
}
