/*	$OpenBSD: kernel.c,v 1.27 2002/08/08 20:17:34 aaron Exp $	*/

/*
 * Copyright 1997-2000 Niels Provos <provos@citi.umich.edu>
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
 * with the PF_ENCAP interface as used by OpenBSD's IPsec implementation.
 * This is the only file which needs to be changed for making Photuris
 * work with other kernel interfaces.
 * The SPI object here can actually hold two SPIs, one for encryption
 * and one for authentication.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: kernel.c,v 1.27 2002/08/08 20:17:34 aaron Exp $";
#endif

#include <time.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <poll.h>

#include <net/pfkeyv2.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ah.h>

#define _KERNEL_C_
#include "photuris.h"
#include "state.h"
#include "attributes.h"
#include "buffer.h"
#include "api.h"
#include "spi.h"
#include "kernel.h"
#include "log.h"
#include "server.h"
#ifdef DEBUG
#include "config.h"
#endif

#define POLL_TIMEOUT	500

#define SPITOINT(x) (((x)[0]<<24) + ((x)[1]<<16) + ((x)[2]<<8) + (x)[3])
#define KERNEL_XF_SET(x) kernel_xf_set(sd, buffer, BUFFER_SIZE, iov, cnt, x)

static int sd;		/* normal PFKEY socket */
static int regsd;	/* PFKEY socket for Register and Acquire */
static int pfkey_seq;
static pid_t pfkey_pid;

struct pfmsg {
	TAILQ_ENTRY(pfmsg) next;

	struct sadb_msg *smsg;
};

TAILQ_HEAD(pflist, pfmsg) pfqueue;

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
			LOG_DBG((LOG_KERNEL, 50,
				 "%s: %s algorithm %d", __func__
				 type == XF_ENC ? "enc" : "auth", id));
			xf[i].flags |= XF_SUP;
			return;
		}
}

/*
 * Parse the supported transforms returned in the SADB_REGISTER response
 */

void
kernel_transform_parse(struct sadb_supported *ssup)
{
	struct sadb_alg *salg = (struct sadb_alg *)(ssup + 1);
	int i, type;

	type = ssup->sadb_supported_exttype == SADB_EXT_SUPPORTED_AUTH ?
		XF_AUTH : XF_ENC;

	for (i = 0; i < ssup->sadb_supported_len - 1; i++, salg++)
		kernel_transform_seen(salg->sadb_alg_id, type);
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
     return (0);
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
	  return (-1); /* We don't know this attribute */

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
	  return (-1);
     if (hmac && !(xf_auth->flags & AH_NEW))
	  return (-1);

     return (0);
}

int
init_kernel(void)
{
	TAILQ_INIT(&pfqueue);
	
	if ((sd = socket(PF_KEY, SOCK_RAW, PF_KEY_V2)) == -1)
		log_fatal("%s: socket(PF_KEY) for IPsec key engine", __func__);
	if ((regsd = socket(PF_KEY, SOCK_RAW, PF_KEY_V2)) == -1)
		log_fatal("%s: socket() for PFKEY register", __func__);

	pfkey_seq = 0;
	pfkey_pid = getpid();

	if (kernel_register(regsd) == -1)
		log_fatal("%s: PFKEY socket registration failed", __func__);

	return (1);
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
	  log_fatal("setsockopt: can not bypass IPsec authentication policy");
     if (setsockopt(sd, IPPROTO_IP, IP_ESP_TRANS_LEVEL,
			(char *)&level, sizeof (int)) == -1)
	  log_fatal("setsockopt: can not bypass IPsec ESP transport policy");
     if (setsockopt(sd, IPPROTO_IP, IP_ESP_NETWORK_LEVEL,
		    (char *)&level, sizeof (int)) == -1)
	  log_fatal("setsockopt: can not bypass IPsec ESP network policy");
}

struct sadb_ext *
pfkey_find_extension(struct sadb_ext *start, void *end, u_int16_t type)
{
	struct sadb_ext *p = start;

	while ((void *)p < end) {
		if (p->sadb_ext_type == type)
			return (p);
		p = (struct sadb_ext *)((u_char *)p + p->sadb_ext_len * 8);
	}
	
	return (NULL);
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
	  return (0);
     }

     if (buffer)
	  return kernel_xf_read(sd, buffer, blen, seq);
     return (1);
}

void
kernel_queue_msg(struct sadb_msg *smsg)
{
	struct pfmsg *pfmsg;

	LOG_DBG((LOG_KERNEL, 50, "%s: queuing message type %d",
		__func__, smsg->sadb_msg_type));

	pfmsg = malloc(sizeof(*pfmsg));
	if (pfmsg == NULL) {
		log_error("%s: malloc", __func__);
		return;
	}

	pfmsg->smsg = malloc(smsg->sadb_msg_len * 8);
	if (pfmsg->smsg == NULL) {
		log_error("%s: malloc", __func__);
		free(pfmsg);
		return;
	}

	memcpy(pfmsg->smsg, smsg, smsg->sadb_msg_len * 8);

	TAILQ_INSERT_TAIL(&pfqueue, pfmsg, next);
}

int
kernel_xf_read(int sd, char *buffer, int blen, int seq)
{
	struct sadb_msg *sres = (struct sadb_msg *)buffer;
	int len, forus;

	/*
	 * Read in response from the kernel. If seq number and/or PID are
	 * given, we need to check PID and sequence number to see if it
	 * really is a message for us.
	 */
	do {
		struct pollfd pfd;

		pfd.fd = sd;
		pfd.events = POLLIN;
		pfd.revents = 0;

		if (poll(&pfd, 1, POLL_TIMEOUT) == -1) {
			log_error("%s: poll", __func__);
			return (0);
		}

		if (!(pfd.revents & POLLIN)) {
			log_print("%s: no reply from pfkey", __func__);
			return (0);
		}

		if (recv(sd, sres, sizeof(*sres), MSG_PEEK) != sizeof(*sres)) {
			log_error("%s: read()", __func__);
			return (0);
		}
		len = sres->sadb_msg_len * 8;
		if (len >= BUFFER_SIZE) {
			log_print("%s: PFKEYV2 message len %d too big", 
			    __func__, len);
			return (0);
		}
		if (read(sd, sres, len) != len) {
			log_error("%s: read()", __func__);
			return (0);
		}
	
		forus = !(sres->sadb_msg_pid &&
			  sres->sadb_msg_pid != pfkey_pid) &&
			!(seq && sres->sadb_msg_seq != seq);

		if (!forus) {
			switch (sres->sadb_msg_type) {
			case SADB_ACQUIRE:
			case SADB_EXPIRE:
				kernel_queue_msg(sres);
				break;
			default:
				LOG_DBG((LOG_KERNEL, 50, 
				    "%s: skipping message type %d", __func__,
				    sres->sadb_msg_type));
				break;
			}
		}
	
	} while (!forus);

	if (sres->sadb_msg_errno) {
		LOG_DBG((LOG_KERNEL, 40, "%s: PFKEYV2 result: %s",
		    __func__, strerror(sres->sadb_msg_errno)));
		errno = sres->sadb_msg_errno;
		return (0);
	}

	return (1);
}

int
kernel_register(int sd)
{
     struct sadb_msg smsg, *sres;
     struct sadb_supported *ssup;
     struct sadb_ext *ext;
     void *end;
     int encfound, authfound;
     struct iovec iov[1];
     int cnt = 0;

     LOG_DBG((LOG_KERNEL, 20, "%s: fd %d", __func__, sd));

     encfound = authfound = 0;

     bzero(&smsg, sizeof(smsg));

     smsg.sadb_msg_len = sizeof(smsg) / 8;
     smsg.sadb_msg_version = PF_KEY_V2;
     smsg.sadb_msg_seq = pfkey_seq++;
     smsg.sadb_msg_pid = pfkey_pid;
     smsg.sadb_msg_type = SADB_REGISTER;
     iov[cnt].iov_base = &smsg;
     iov[cnt++].iov_len = sizeof(smsg);

     /* Register for ESP */
     smsg.sadb_msg_satype = SADB_SATYPE_ESP;
     if (!kernel_xf_set(regsd, buffer, BUFFER_SIZE, iov, cnt,
			smsg.sadb_msg_len*8)) {
	  log_error("%s: kernel_xf_set()", __func__);
	  return (-1);
     }

     sres = (struct sadb_msg *)buffer;
     ext = (struct sadb_ext *)(sres + 1);
     end = (u_char *)sres + sres->sadb_msg_len * 8;
     ssup = (struct sadb_supported *)
	     pfkey_find_extension(ext, end, SADB_EXT_SUPPORTED_AUTH);
     if (ssup) {
	     kernel_transform_parse(ssup);
	     authfound = 1;
     }
     ssup = (struct sadb_supported *)
	     pfkey_find_extension(ext, end, SADB_EXT_SUPPORTED_ENCRYPT);
     if (ssup) {
	     kernel_transform_parse(ssup);
	     encfound = 1;
     }

     /* Register for AH */
     smsg.sadb_msg_satype = SADB_SATYPE_AH;
     smsg.sadb_msg_seq = pfkey_seq++;
     if (!kernel_xf_set(regsd, buffer, BUFFER_SIZE, iov, cnt,
			smsg.sadb_msg_len*8)) {
	  log_error("%s: kernel_xf_set()", __func__);
	  return (-1);
     }

     ext = (struct sadb_ext *)(sres + 1);
     end = (u_char *)sres + sres->sadb_msg_len * 8;

     ssup = (struct sadb_supported *)
	     pfkey_find_extension(ext, end, SADB_EXT_SUPPORTED_AUTH);
     if (ssup) {
	     kernel_transform_parse(ssup);
	     authfound = 1;
     }
     ssup = (struct sadb_supported *)
	     pfkey_find_extension(ext, end, SADB_EXT_SUPPORTED_ENCRYPT);
     if (ssup) {
	     kernel_transform_parse(ssup);
	     encfound = 1;
     }

     if (!authfound || !encfound) {
	     log_print("%s: SADB_REGISTER without supported algs %s %s",
		       __func__, encfound == 0 ? "encryption" : "",
		       authfound == 0 ? "authentication" : "");
	     return (-1);
     }
	
     return (0);
}

u_int32_t
kernel_reserve_spi(char *src, char *dst, int options)
{
     u_int32_t spi;
     int proto;

     LOG_DBG((LOG_KERNEL, 40, "%s: %s %s %s", __func__, src,
	      options & IPSEC_OPT_ENC ? "ESP" : "",
	      options & IPSEC_OPT_AUTH ? "AH" : ""));

     if ((options & (IPSEC_OPT_ENC|IPSEC_OPT_AUTH)) !=
	 (IPSEC_OPT_ENC|IPSEC_OPT_AUTH)) {
	  switch(options & (IPSEC_OPT_ENC|IPSEC_OPT_AUTH)) {
	  case IPSEC_OPT_ENC:
	       proto = IPPROTO_ESP;
	       break;
	  default:
	       proto = IPPROTO_AH;
	       break;
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
	  log_error("%s: kernel_xf_set()", __func__);
	  return (0);
     }

     sres = (struct sadb_msg *)buffer;
     ssa = (struct sadb_sa *)(sres + 1);
     if (ssa->sadb_sa_exttype != SADB_EXT_SA) {
	  log_print(
	  	"%s: SADB_GETSPI did not return a SADB_EXT_SA struct: %d",
		__func__, ssa->sadb_sa_exttype);
	  return (0);
     }

     LOG_DBG((LOG_KERNEL, 40, "%s: %s, %08x -> %08x", __func__,
	      srcaddress, spi, ntohl(ssa->sadb_sa_spi)));

     return (ntohl(ssa->sadb_sa_spi));
}

int
kernel_add_lifetime(struct sadb_msg *sa, struct iovec *iov, int seconds)
{
	static struct sadb_lifetime slh, sls;
	int cnt = 0;

	bzero(&slh, sizeof(slh));
	bzero(&sls, sizeof(sls));

	slh.sadb_lifetime_len = sizeof(slh) / 8;
	slh.sadb_lifetime_exttype = SADB_EXT_LIFETIME_HARD;
	slh.sadb_lifetime_allocations = 0;
	slh.sadb_lifetime_bytes = 10000000;   /* lots of bytes */
	slh.sadb_lifetime_addtime = seconds;
	sa->sadb_msg_len += slh.sadb_lifetime_len;

	iov[cnt].iov_base = &slh;
	iov[cnt++].iov_len = sizeof(slh);

	sls.sadb_lifetime_len = sizeof(sls) / 8;
	sls.sadb_lifetime_exttype = SADB_EXT_LIFETIME_SOFT;
	sls.sadb_lifetime_allocations = 0;
	sls.sadb_lifetime_bytes = 9000000;   /* lots of bytes */
	sls.sadb_lifetime_addtime = seconds * 9 / 10;
	sa->sadb_msg_len += slh.sadb_lifetime_len;

	iov[cnt].iov_base = &sls;
	iov[cnt++].iov_len = sizeof(sls);

	return (cnt);
}

int
kernel_ah(attrib_t *ob, struct spiob *SPI, u_int8_t *secrets, int hmac)
{
     struct sadb_msg sa;
     struct sadb_address sad1;
     struct sadb_address sad2;
     struct sadb_sa sr;
     struct sadb_key sk;
     struct sockaddr_in src;
     struct sockaddr_in dst;
     struct iovec iov[20];
     int cnt = 0;
     transform *xf = kernel_get_transform(ob->id);
     time_t now = time(NULL);

     if (xf == NULL || !(xf->flags & XF_AUTH)) {
	  log_print("%s: %d is not an auth transform", __func__, ob->id);
	  return (-1);
     }

     bzero(&sa, sizeof(sa));
     bzero(&sad1, sizeof(sad1));
     bzero(&sad2, sizeof(sad2));
     bzero(&sr, sizeof(sr));
     bzero(&sk, sizeof(sk));
     bzero(&src, sizeof(src));
     bzero(&dst, sizeof(dst));

     sa.sadb_msg_len = sizeof(sa) / 8;
     sa.sadb_msg_version = PF_KEY_V2;
     sa.sadb_msg_type = SPI->flags & SPI_OWNER ?
	  SADB_UPDATE : SADB_ADD;
     sa.sadb_msg_satype = SADB_SATYPE_AH;
     sa.sadb_msg_seq = pfkey_seq++;
     sa.sadb_msg_pid = pfkey_pid;
     iov[cnt].iov_base = &sa;
     iov[cnt++].iov_len = sizeof(sa);

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

     sr.sadb_sa_len = sizeof(sr) / 8;
     sr.sadb_sa_exttype = SADB_EXT_SA;
     sr.sadb_sa_spi = htonl(SPITOINT(SPI->SPI));
     sr.sadb_sa_replay = !hmac ? 0 : 32;
     sr.sadb_sa_state = SADB_SASTATE_MATURE;
     sr.sadb_sa_auth = xf->kernel_id;
     sr.sadb_sa_encrypt = 0;
     if (!hmac)
	  sr.sadb_sa_flags |= SADB_X_SAFLAGS_NOREPLAY;
     sa.sadb_msg_len += sr.sadb_sa_len;

     iov[cnt].iov_base = &sr;
     iov[cnt++].iov_len = sizeof(sr);

     cnt += kernel_add_lifetime(&sa, &iov[cnt], SPI->lifetime - now);

     sk.sadb_key_len = (sizeof(sk) + ob->klen + 7) / 8;
     sk.sadb_key_exttype = SADB_EXT_KEY_AUTH;
     sk.sadb_key_bits = ob->klen * 8;
     sa.sadb_msg_len += sk.sadb_key_len;

     iov[cnt].iov_base = &sk;
     iov[cnt++].iov_len = sizeof(sk);
     iov[cnt].iov_base = secrets;
     iov[cnt++].iov_len = ((ob->klen + 7) / 8) * 8;

     LOG_DBG((LOG_KERNEL, 35, "%s: %08x", __func__, ntohl(sr.sadb_sa_spi)));

     if (!KERNEL_XF_SET(sa.sadb_msg_len * 8)) {
	  log_error("%s: kernel_xf_set()", __func__);
	  return (-1);
     }
     return (ob->klen);
}

int
kernel_esp(attrib_t *ob, attrib_t *ob2, struct spiob *SPI, u_int8_t *secrets)
{
     struct sadb_msg sa;
     struct sadb_address sad1;
     struct sadb_address sad2;
     struct sadb_sa sr;
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
	       log_print("%s: No encryption after auth given", __func__);
	       return (-1);
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
	  log_print("%s: No encryption transform given", __func__);
	  return (-1);
     }

     xf_enc = kernel_get_transform(attenc->id);
     if ((xf_enc->flags & ESP_OLD) && attauth != NULL) {
	  log_print("%s: Old ESP does not support AH", __func__);
	  return (-1);
     }

     if (attauth != NULL)
	  xf_auth = kernel_get_transform(attauth->id);

     bzero(&sa, sizeof(sa));
     bzero(&sad1, sizeof(sad1));
     bzero(&sad2, sizeof(sad2));
     bzero(&sr, sizeof(sr));
     bzero(&sk1, sizeof(sk1));
     bzero(&sk2, sizeof(sk2));
     bzero(&src, sizeof(src));
     bzero(&dst, sizeof(dst));

     sa.sadb_msg_len = sizeof(sa) / 8;
     sa.sadb_msg_version = PF_KEY_V2;
     sa.sadb_msg_type = SPI->flags & SPI_OWNER ?
	  SADB_UPDATE : SADB_ADD;
     sa.sadb_msg_satype = SADB_SATYPE_ESP;
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
     if (xf_enc->flags & ESP_OLD) {
	  sr.sadb_sa_flags |= SADB_X_SAFLAGS_HALFIV;
	  sr.sadb_sa_flags |= SADB_X_SAFLAGS_RANDOMPADDING;
	  sr.sadb_sa_flags |= SADB_X_SAFLAGS_NOREPLAY;
     }
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

     cnt += kernel_add_lifetime(&sa, &iov[cnt], SPI->lifetime - now);

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

     LOG_DBG((LOG_KERNEL, 35, "%s: %08x", __func__, ntohl(sr.sadb_sa_spi)));

     if (!KERNEL_XF_SET(sa.sadb_msg_len * 8)) {
	  log_error("%s: kernel_xf_set()", __func__);
	  return (-1);
     }

     return attenc->klen + (attauth ? attauth->klen : 0);
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


     LOG_DBG((LOG_KERNEL, 30, "%s: %08x", __func__, spi));

     if (!KERNEL_XF_SET(sa.sadb_msg_len * 8) && errno != ESRCH) {
	  log_error("%s: kernel_xf_set()", __func__);
	  return (-1);
     }

     return (1);
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
		    log_print("%s: Unknown attribute %d for ESP",
		    	__func__, esp[count]);
		    return (-1);
	       }
	       if (atesp == NULL && attprop->type == AT_ENC)
		    atesp = attprop;
	       else if(atah == NULL && (attprop->type & AT_AUTH))
		    atah = attprop;

	       count += esp[count+1]+2;
	  }
	  if (atesp == NULL) {
	       log_print("%s: No encryption attribute in ESP section for SA(%08x, %s->%s)",
		    __func__, 
		    SPITOINT(SPI->SPI), SPI->local_address, SPI->address);
	       return (-1);
	  }

	  offset = kernel_esp(atesp, atah, SPI, secrets);
	  if (offset == -1)
	       return (-1);
	  secrets += offset;
     }

     if (ah != NULL) {
	  int count = 0, hmac = 0;
	  attrib_t *atah = NULL;

	  while (count < ahsize) {
	       if ((attprop = getattrib(ah[count])) == NULL) {
		    log_print("%s: Unknown attribute %d for AH", __func__,
			      ah[count]);
		    return (-1);
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
	       log_print("%s: No authentication attribute in AH section for SA(%08x, %s->%s)",
		    __func__,
			 SPITOINT(SPI->SPI), SPI->local_address, SPI->address);
	       return (-1);
	  }

	  offset = kernel_ah(atah, SPI, secrets, hmac);
	  if (offset == -1)
	       return (-1);
	  secrets += offset;
     }

     if (esp != NULL) {
	  proto = IPPROTO_ESP;
	  SPI->flags |= SPI_ESP;
     } else {
	  proto = IPPROTO_AH;
	  SPI->flags &= ~SPI_ESP;
     }

    /*
     * Inform the kernel that we obtained the requested SA
     */
     kernel_notify_result(st, SPI, proto);

     /* Erase keys */
     bzero(SPI->sessionkey, SPI->sessionkeysize);
     free(SPI->sessionkey);
     SPI->sessionkey = NULL; SPI->sessionkeysize = 0;

     return (1);
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
	  if (kernel_delete_spi(p, SPITOINT(ospi->SPI), IPPROTO_ESP) == -1)
	       log_print("%s: kernel_delete_spi() failed", __func__);
     }
	
     if (ah != NULL) {
	  if (kernel_delete_spi(p, SPITOINT(ospi->SPI), IPPROTO_AH) == -1)
	       log_print("%s: kernel_delete_spi() failed", __func__);
     }

     return (1);
}

void
kernel_dispatch_notify(struct sadb_msg *sres)
{
	LOG_DBG((LOG_KERNEL, 60, "%s: Got PFKEYV2 message: type %d",
		__func__, sres->sadb_msg_type));

	switch (sres->sadb_msg_type) {
	case SADB_EXPIRE:
		LOG_DBG((LOG_KERNEL, 55, "%s: Got SA Expiration", __func__));
		kernel_handle_expire(sres);
		break;
	case SADB_ACQUIRE:
		LOG_DBG((LOG_KERNEL, 55, 
			 "%s: Got Notify SA Request (SADB_ACQUIRE): %d",
			 __func__, 
			 sres->sadb_msg_len * 8));
		LOG_DBG_BUF((LOG_KERNEL, 60, "acquire buf",
			     (u_char *)sres, sres->sadb_msg_len * 8));
	
		
		kernel_request_sa(sres);
		break;
	default:
		/* discard silently */
		return;
	}
}

void
kernel_handle_queue()
{
	struct pfmsg *pfmsg;

	while ((pfmsg = TAILQ_FIRST(&pfqueue))) {
		TAILQ_REMOVE(&pfqueue, pfmsg, next);

		kernel_dispatch_notify(pfmsg->smsg);

		free(pfmsg->smsg);
		free(pfmsg);
	}
}

/*
 * Handles Notifies from the kernel, which can include Requests for new
 * SAs, soft and hard expirations for already established SAs.
 */

void
kernel_handle_notify(int sd)
{
	struct sadb_msg *sres = (struct sadb_msg *)buffer;
	size_t len;

	if (!kernel_xf_read(regsd, buffer, BUFFER_SIZE, 0)) {
		LOG_DBG((LOG_KERNEL, 65, "%s: nothing to read", __func__));
		return;
	}

	len = sres->sadb_msg_len * 8;
	sres = malloc(len);
	if (!sres) {
		log_error("%s: malloc", __func__);
		return;
	}
	memcpy(sres, buffer, len);

	kernel_dispatch_notify(sres);

	free(sres);
}

struct sadb_msg *
pfkey_askpolicy(int seq)
{
	struct sadb_msg smsg;
	struct sadb_x_policy policy;
	struct iovec iov[2];
	int cnt = 0;

	bzero(&smsg, sizeof(smsg));

	/* Ask the kernel for the matching policy */
	smsg.sadb_msg_len = sizeof(smsg) / 8;
	smsg.sadb_msg_version = PF_KEY_V2;
	smsg.sadb_msg_seq = pfkey_seq++;
	smsg.sadb_msg_pid = pfkey_pid;
	smsg.sadb_msg_type = SADB_X_ASKPOLICY;
	iov[cnt].iov_base = &smsg;
	iov[cnt++].iov_len = sizeof(smsg);

	memset(&policy, 0, sizeof(policy));
	policy.sadb_x_policy_exttype = SADB_X_EXT_POLICY;
	policy.sadb_x_policy_len = sizeof(policy) / 8;
	policy.sadb_x_policy_seq = seq;
	iov[cnt].iov_base = &policy;
	iov[cnt++].iov_len = sizeof(policy);
	smsg.sadb_msg_len += sizeof(policy) / 8;

	if (!kernel_xf_set(regsd, buffer, BUFFER_SIZE, iov, cnt,
			   smsg.sadb_msg_len*8)) {
		log_error("%s: kernel_xf_set", __func__);
		return (NULL);
	}

	return ((struct sadb_msg *)buffer);
}

int
kernel_handle_expire(struct sadb_msg *sadb)
{
	struct sadb_sa *sa;
	struct sadb_address *dst;
	char dstbuf[NI_MAXHOST];
	struct stateob *st;
	time_t tm;
	struct sockaddr *dstaddr;
	struct sadb_lifetime *life;
	struct sadb_ext *ext = (struct sadb_ext *)(sadb + 1);
	struct spiob *spi;
	void *end;

	end = (struct sadb_ext *)((u_char *)sadb + sadb->sadb_msg_len * 8);

	sa = (struct sadb_sa *)pfkey_find_extension(ext, end, SADB_EXT_SA);
	if (sa == NULL) {
		log_print("%s: no SA extension found", __func__);
		return (-1);
	}

	dst = (struct sadb_address *)
		pfkey_find_extension(ext, end, SADB_EXT_ADDRESS_DST);
	if (dst == NULL) {
		log_print(
			  "%s: no destination address extension found",
			  __func__);
		return (-1);
	}
	dstaddr = (struct sockaddr *)(dst + 1);

	life = (struct sadb_lifetime *)
		pfkey_find_extension(ext, end, SADB_EXT_LIFETIME_HARD);
	if (life == NULL)
		life = (struct sadb_lifetime *)
			pfkey_find_extension(ext, end, SADB_EXT_LIFETIME_SOFT);
	if (life == NULL) {
		log_print(
			  "%s: no lifetime extension found",
			  __func__);
		return (-1);
	}

	switch (dstaddr->sa_family) {
	case AF_INET:
		if (inet_ntop (AF_INET, &((struct sockaddr_in *)dstaddr)->sin_addr,
			       dstbuf, sizeof(dstbuf)) == NULL) {
			log_error ("%s: inet_ntop failed", __func__);
			return (-1);
		}
		break;
	default:
		log_error(
			  "%s: unsupported address family %d",
			  __func__,
			  dstaddr->sa_family);
		return (-1);
	}

	LOG_DBG((LOG_KERNEL, 30, "%s: %s dst %s SPI %x sproto %d", __func__,
	    life->sadb_lifetime_exttype == SADB_EXT_LIFETIME_SOFT ? "SOFT"
	    : "HARD", dstbuf,
	    ntohl (sa->sadb_sa_spi), sadb->sadb_msg_satype));

	spi = spi_find(dstbuf, (u_char *)&sa->sadb_sa_spi);
	if (spi == NULL) {
		LOG_DBG((LOG_KERNEL, 35, 
			 "%s: can't find %s SPI %x", __func__,
			 dstbuf, ntohl(sa->sadb_sa_spi)));
		return (-1);
	}

	switch(life->sadb_lifetime_exttype) {
	case SADB_EXT_LIFETIME_HARD:
		LOG_DBG((LOG_KERNEL, 35, "%s: removing %s SPI %x", __func__,
				 dstbuf, ntohl(sa->sadb_sa_spi)));
		spi_unlink(spi);
		break;
	case SADB_EXT_LIFETIME_SOFT:
		life = (struct sadb_lifetime *)
			pfkey_find_extension(ext, end,
					     SADB_EXT_LIFETIME_CURRENT);
		if (life == NULL) {
			log_print("%s: no current lifetime", __func__);
			return (-1);
		}

		if (!life->sadb_lifetime_bytes) {
			LOG_DBG((LOG_KERNEL, 45, 
				 "%s: SPI %x not been used, skipping update",
				 __func__,
				 ntohl(sa->sadb_sa_spi)));
			return (0);
		}

		if (spi->flags & SPI_OWNER) {
			spi_update(global_socket,
				   (u_int8_t *)&sa->sadb_sa_spi);
			return (0);
		}

		/*
		 * Try to find an already established exchange which is
		 * still valid.
		 */

		st = state_find(dstbuf);

		tm = time(NULL);
		while (st != NULL &&
		       (st->lifetime <= tm || st->phase < SPI_UPDATE))
			st = state_find_next(st, dstbuf);

		if (st == NULL) {
			int type = spi->flags & SPI_ESP ?
				IPSEC_OPT_ENC : IPSEC_OPT_AUTH;

			LOG_DBG((LOG_KERNEL, 45, 
				 "%s: starting new exchange to %s",
				 __func__,
				 spi->address));
			kernel_new_exchange(spi->address, type);
		}

		break;
	default:
		log_print("%s: unknown extension type %d", __func__,
			  life->sadb_lifetime_exttype);
		return (-1);
	}

	return (0);
}

int
kernel_new_exchange(char *address, int type)
{
	struct stateob *st;

	/* No established exchange found, start a new one */
	if ((st = state_new()) == NULL) {
		log_print(
			  "%s: state_new() failed for remote ip %s", __func__,
			  address);
		return (-1);
	}

	/* Set up the state information */
	strncpy(st->address, address, sizeof(st->address) - 1);
	st->port = global_port;
	st->sport = 0;
	st->dport = 0;
	st->protocol = 0;

	st->flags = IPSEC_NOTIFY;

	st->flags |= type;

	if (start_exchange(global_socket, st, st->address,
			   st->port) == -1) {
		log_print("%s: start_exchange() - informing kernel of failure",
		    __func__);
		/* Inform kernel of our failure */
		kernel_notify_result(st, NULL, 0);
		state_value_reset(st);
		free(st);
		return (-1);
	} else
		state_insert(st);

	return (0);
}

/*
 * Tries to establish a new SA according to the information in a
 * REQUEST_SA notify message received from the kernel.
 */

int
kernel_request_sa(struct sadb_msg *sadb)
{
	struct stateob *st;
	time_t tm;
	struct sadb_address *dst, *src;
	struct sockaddr *dstaddr;
	struct sadb_ext *ext = (struct sadb_ext *)(sadb + 1);
	char srcbuf[NI_MAXHOST], dstbuf[NI_MAXHOST];
	void *end;

	memset(srcbuf, 0, sizeof(srcbuf));
	memset(dstbuf, 0, sizeof(dstbuf));

	end = (struct sadb_ext *)((u_char *)sadb + sadb->sadb_msg_len * 8);

	dst = (struct sadb_address *)
		pfkey_find_extension(ext, end, SADB_EXT_ADDRESS_DST);
	src = (struct sadb_address *)
		pfkey_find_extension(ext, end, SADB_EXT_ADDRESS_SRC);

	if (!dst)
		return (-1);

	dstaddr = (struct sockaddr *)(dst + 1);
	switch (dstaddr->sa_family) {
	case AF_INET:
		if (inet_ntop(AF_INET,
			      &((struct sockaddr_in *)dstaddr)->sin_addr,
			      dstbuf, sizeof(dstbuf)) == NULL) {
			log_error ("%s: inet_ntop failed", __func__);
			return (-1);
		}
		break;
	default:
		log_error("%s: unsupported address family %d", __func__,
			  dstaddr->sa_family);
		return (-1);
	}
	
	LOG_DBG((LOG_KERNEL, 20, "%s: dst: %s", __func__, dstbuf));

	/* Try to find an already established exchange which is still valid */
	st = state_find(dstbuf);

	tm = time(NULL);
	while (st != NULL && (st->lifetime <= tm || st->phase < SPI_UPDATE))
		st = state_find_next(st, dstbuf);

	if (st) {
		struct sockaddr_in sin;

		/*
		 * We need different attributes for this exchange, send
		 * an SPI_NEEDED message.
		 */

		packet_size = PACKET_BUFFER_SIZE;
		if (photuris_spi_needed(st, packet_buffer, &packet_size,
					st->uSPIattrib,
					st->uSPIattribsize) == -1) {
			log_print("%s: photuris_spi_update()", __func__);
			return (-1);
		}

		/* Send the packet */
		sin.sin_port = htons(st->port);
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = inet_addr(st->address);
		
		if (sendto(global_socket, packet_buffer, packet_size, 0,
			   (struct sockaddr *)&sin, sizeof(sin)) != packet_size) {
			log_error("%s: sendto()", __func__);
		}
	} else {
		int type = sadb->sadb_msg_satype == SADB_SATYPE_ESP ?
			IPSEC_OPT_ENC : IPSEC_OPT_AUTH;

		return (kernel_new_exchange(dstbuf, type));
	}


	return (0);
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
     log_error("kernel_xf_set() in kernel_notify_result()"); */
}
