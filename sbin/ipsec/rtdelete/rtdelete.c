/*
 * The author of this code is John Ioannidis, ji@tla.org,
 * 	(except when noted otherwise).
 *
 * This code was written for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis, kermit@forthnet.gr.
 *
 * Copyright (C) 1995, 1996, 1997 by John Ioannidis and Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NEITHER AUTHOR MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

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

 

#define SENO_EOL        0x00            /* End of Options, or placeholder */
#define SENO_NOP        0x01            /* No Operation. Skip */
#define SENO_NAME       0x02            /* tunnel name, NUL-terminated */
#define SENO_TDB        0x03            /* tunnel descriptor block address */
#define SENO_IFN        0x04            /* Encap interface number */
#define SENO_IFIP4A     0x05            /* Encap interface IPv4 address */
#define SENO_IPSA       0x06            /* Encap interface generic sockaddr */


#define IFT_ENC 0x37
#define INET


#include "net/encap.h"

/*
 * The numbers below are arbitrary. They have been chosen for their
 * mnemonic value. If sen_len is larger than what would be expected from
 * the length of the data that follow, then TLV-triplets follow the 
 * addresses containing system-depended information.
 */


char buf[2048];

main(argc, argv)
int argc;
char **argv;
{
	int sd;

	struct rt_msghdr *rtm;
	struct sockaddr_encap *dst, *msk, *gw;
	struct sockaddr_dl *dl;
	u_char *opts;

	if (argc != 12)
	  fprintf(stderr, "usage: %s isrc isrcmask idst idstmask osrc odst spi if proto sport dport\n", argv[0]), exit(1);
	
	sd = socket(PF_ROUTE, SOCK_RAW, AF_UNSPEC);
	if (sd < 0)
	  perror("socket"), exit(1);
	
	rtm = (struct rt_msghdr *)(&buf[0]);
	dst = (struct sockaddr_encap *)(&buf[sizeof (*rtm)]);
	gw = (struct sockaddr_encap *)(&buf[sizeof (*rtm) + SENT_IP4_LEN]);
/*	opts = (u_char *)(&buf[sizeof (*rtm) + 2*SENT_IP4_LEN]); */
	msk = (struct sockaddr_encap *)(&buf[sizeof (*rtm) + SENT_IP4_LEN + SENT_IPSP_LEN]);
/*	dl = (struct sockaddr_dl *)(&buf[sizeof (*rtm) + 3*SENT_IP4_LEN + 12]); */
	
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = RTM_DELETE;
	rtm->rtm_index = 0;
	rtm->rtm_pid = getpid();
	rtm->rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK /* | RTA_IFP */;
	rtm->rtm_errno = 0;
	rtm->rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
	rtm->rtm_inits = 0;
	
	dst->sen_len = SENT_IP4_LEN;
	dst->sen_family = AF_ENCAP;
	dst->sen_type = SENT_IP4;
	dst->sen_ip_src.s_addr = inet_addr(argv[1]);
	dst->sen_ip_dst.s_addr = inet_addr(argv[3]);
	dst->sen_proto = dst->sen_sport = dst->sen_dport = 0;

	if (atoi(argv[9]) >= 0)
	{
		dst->sen_proto = atoi(argv[9]);
		msk->sen_proto = 0xff;
		if (atoi(argv[10]) >= 0)
		{
			dst->sen_sport = atoi(argv[10]);
			msk->sen_sport = 0xffff;
		}
		if (atoi(argv[11]) >= 0)
		{
			dst->sen_dport = atoi(argv[11]);
			msk->sen_dport = 0xffff;
		}
	}

	gw->sen_len = SENT_IPSP_LEN;
	gw->sen_family = AF_ENCAP;
	gw->sen_type = SENT_IPSP;
	gw->sen_ipsp_src.s_addr = inet_addr(argv[5]);
	gw->sen_ipsp_dst.s_addr = inet_addr(argv[6]);
	gw->sen_ipsp_spi = htonl(strtoul(argv[7], NULL, 16));
	gw->sen_ipsp_ifn = atoi(argv[8]);

/*
	opts[0] = SENO_IFN;
	opts[1] = 3;
	opts[2] = 2;
	opts[3] = SENO_NOP;
	opts[4] = SENO_NOP;
	opts[5] = SENO_NOP;
	opts[6] = SENO_SPI;
	opts[7] = 4;
	opts[8] = 0x12;
	opts[9] = 0x34;
	opts[10] = 0x56;
	opts[11] = 0x78;
*/
	msk->sen_len = SENT_IP4_LEN;
	msk->sen_family = AF_ENCAP;
	msk->sen_type = SENT_IP4;
	msk->sen_ip_src.s_addr = inet_addr(argv[2]);
	msk->sen_ip_dst.s_addr = inet_addr(argv[4]);

/*
	dl->sdl_len = 12;
	dl->sdl_family = AF_DLI;
	dl->sdl_index = 0;
	dl->sdl_type = IFT_ENC;
	dl->sdl_nlen = 4;
	dl->sdl_alen = 0;
	dl->sdl_slen = 0;
	dl->sdl_data[0] = 'e';
	dl->sdl_data[1] = 'n';
	dl->sdl_data[2] = 'c';
	dl->sdl_data[3] = '2';
*/

	rtm->rtm_msglen = sizeof (*rtm) + dst->sen_len + gw->sen_len + msk->sen_len /* + dl->sdl_len */ ;
	
	if (write(sd, (caddr_t)buf, rtm->rtm_msglen) < 0)
	  perror("write");
}

	
	
