/*	$OpenBSD: netbios.h,v 1.3 2000/10/03 14:31:55 ho Exp $	*/

/*
 * NETBIOS protocol formats
 *
 * @(#) $Header: /home/cvs/src/usr.sbin/tcpdump/netbios.h,v 1.3 2000/10/03 14:31:55 ho Exp $
 */

struct p8022Hdr {
    u_char	dsap;
    u_char	ssap;
    u_char	flags;
};

#define	p8022Size	3		/* min 802.2 header size */

#define UI		0x03		/* 802.2 flags */

