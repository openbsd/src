/*
 * NETBIOS protocol formats
 *
 * @(#) $Header: /home/cvs/src/usr.sbin/tcpdump/netbios.h,v 1.2 1996/12/12 16:22:49 bitblt Exp $
 */

struct p8022Hdr {
    u_char	dsap;
    u_char	ssap;
    u_char	flags;
};

#define	p8022Size	3		/* min 802.2 header size */

#define UI		0x03		/* 802.2 flags */

