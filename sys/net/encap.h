/*	$OpenBSD: encap.h,v 1.13 1998/05/24 14:13:59 provos Exp $	*/

/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and 
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece, 
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Copyright (C) 1995, 1996, 1997, 1998 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

/*
 * encap.h
 *
 * Declarations useful in the encapsulation code.
 */

/* Sysctl definitions */

#define ENCAPCTL_ENCDEBUG	1
#define ENCAPCTL_MAXID		2

#define ENCAPCTL_NAMES {\
	{ 0, 0 }, \
	{ "encdebug", CTLTYPE_INT }, \
}

/*
 * Definitions for encapsulation-related phenomena.
 *
 * A lot of encapsulation protocols (ipip, swipe, ip_encap, ipsp, etc.)
 * select their tunnel based on the destination (and sometimes the source)
 * of the packet. The encap address/protocol family provides a generic
 * mechanism for specifying tunnels.
 */

/*
 * A tunnel is characterized by which source/destination address pairs
 * (with netmasks) it is valid for (the "destination" as far as the
 * routing code is concerned), and what the source (local) and destination
 * (remote) endpoints of the tunnel, and the SPI, should be (the "gateway"
 * as far as the routing code is concerned.
 */
  
struct sockaddr_encap
{
    u_int8_t	sen_len;		/* length */
    u_int8_t	sen_family;		/* AF_ENCAP */
    u_int16_t	sen_type;		/* see SENT_* */
    union
    {
	u_int8_t	Data[16];	/* other stuff mapped here */

	struct				/* SENT_IP4 */
	{
	    struct in_addr Src;
	    struct in_addr Dst;
	    u_int16_t Sport;
	    u_int16_t Dport;
	    u_int8_t Proto;
	    u_int8_t Filler[3];
	} Sip4;

	struct				/* SENT_IPSP */
	{
	    struct in_addr Dst;
	    u_int32_t Spi;
	    u_int8_t Sproto;
	    u_int8_t Filler[7];
	} Sipsp;
    } Sen;
};

#define PFENCAP_VERSION_0	0
#define PFENCAP_VERSION_1	1

#define sen_data	Sen.Data
#define sen_ip_src	Sen.Sip4.Src
#define sen_ip_dst	Sen.Sip4.Dst
#define sen_proto	Sen.Sip4.Proto
#define sen_sport	Sen.Sip4.Sport
#define sen_dport	Sen.Sip4.Dport
#define sen_ipsp_dst	Sen.Sipsp.Dst
#define sen_ipsp_spi	Sen.Sipsp.Spi
#define sen_ipsp_sproto	Sen.Sipsp.Sproto

/*
 * The "type" is really part of the address as far as the routing
 * system is concerned. By using only one bit in the type field
 * for each type, we sort-of make sure that different types of
 * encapsulation addresses won't be matched against the wrong type.
 * 
 */

#define SENT_IP4	0x0001		/* data is two struct in_addr */
#define SENT_IPSP	0x0002		/* data as in IP4 plus SPI */

/*
 * SENT_HDRLEN is the length of the "header"
 * SENT_*_LEN are the lengths of various forms of sen_data
 * SENT_*_OFF are the offsets in the sen_data array of various fields
 */

#define SENT_HDRLEN	(2 * sizeof(u_int8_t) + sizeof(u_int16_t))

#define SENT_IP4_SRCOFF	(0)
#define SENT_IP4_DSTOFF (sizeof (struct in_addr))

#define SENT_IP4_LEN	20
#define SENT_IPSP_LEN	20

/*
 * For encapsulation routes are possible not only for the destination
 * address but also for the protocol, source and destination ports
 * if available
 */

struct route_enc {
    struct rtentry *re_rt;
    struct sockaddr_encap re_dst;
};

/*
 * Tunnel descriptors are setup and torn down using a socket of the 
 * AF_ENCAP domain. The following defines the messages that can
 * be sent down that socket.
 */
struct encap_msghdr
{
    u_int16_t	em_msglen;		/* message length */
    u_int8_t	em_version;		/* for future expansion */
    u_int8_t	em_type;		/* message type */
    u_int32_t   foo;                    /* Alignment to 64 bit */
    union
    {
	/* 
	 * This is used to set/change the attributes of an SPI. If oSrc and
	 * oDst are set to non-zero values, the SPI will also do IP-in-IP
	 * encapsulation (tunneling). If only one of them is set, an error
	 * is returned. Both zero implies transport mode.
	 */
	struct
	{
	    u_int32_t      Spi;		/* SPI */
	    int32_t        Alg;		/* Algorithm to use */
	    struct in_addr Dst;		/* Destination address */
	    struct in_addr Src;		/* This is used to set our source
					 * address when the outgoing packet
				         * does not have a source address 
					 * (is zero). */
	    struct in_addr oSrc;	 /* Outter header source address */
	    struct in_addr oDst;	 /* Same, for destination address */
	    u_int64_t      First_Use_Hard; /* Expire relative to first use */
	    u_int64_t      First_Use_Soft;
	    u_int64_t      Expire_Hard;	/* Expire at fixed point in time */
	    u_int64_t      Expire_Soft;
	    u_int64_t      Bytes_Hard;	/* Expire after bytes recved/sent */
	    u_int64_t      Bytes_Soft;
	    u_int64_t      Packets_Hard; /* Expire after packets recved/sent */
	    u_int64_t      Packets_Soft;
	    int32_t	   TTL;		/* When tunneling, what TTL to use.
					 * If set to IP4_SAME_TTL, the ttl
					 * from the encapsulated packet will
					 * be copied. If set to IP4_DEFAULT_TTL,
					 * the system default TTL will be used.
					 * If set to anything else, then the
					 * ttl used will be TTL % 256 */
	    u_int16_t      Satype;
	    u_int8_t       Sproto;	/* ESP or AH */
	    u_int8_t	   Foo;		/* Alignment */
	    u_int8_t       Dat[1];	/* Data */
	} Xfm;

	/*
 	 * For expiration notifications, the kernel fills in
	 * Notification_Type, Spi, Dst and Sproto, Src and Satype.
  	 * No direct response is expected.
	 *
 	 * For SA Requests, the kernel fills in
	 * Notification_Type, MsgID, Dst, Satype, (and optionally
	 * Protocol, Src, Sport, Dport and UserID).
 	 *
	 */
	struct				/* kernel->userland notifications */
	{
	    u_int32_t      Notification_Type;
	    u_int32_t      MsgID;	/* Request ID */
	    u_int32_t      Spi;		
	    struct in_addr Dst;		/* Peer */
	    struct in_addr Src;		/* Might have our local address */
	    u_int16_t      Sport;	/* Source port */
            u_int16_t      Dport;	/* Destination port */
	    u_int8_t       Protocol;	/* Transport protocol */
	    u_int8_t       Sproto;	/* IPsec protocol */
	    u_int16_t      Satype;	/* SA type */
	    u_int32_t      Foo;		/* Alignment */
	    u_int8_t       UserID[1];	/* Might be used to indicate user */
	} Notify;

	/* Link two SPIs */
	struct
	{
	    u_int32_t        Spi;	/* SPI */
	    u_int32_t        Spi2;
	    struct in_addr   Dst;	/* Dest */
	    struct in_addr   Dst2;
	    u_int8_t	     Sproto; 	/* IPsec protocol */
	    u_int8_t	     Sproto2;
	} Rel;

	/* Enable/disable an SA for a session */
	struct
	{
	    u_int32_t      Spi;
	    struct in_addr Dst;
	    struct in_addr iSrc;	/* Source... */
	    struct in_addr iDst;	/* ...and destination in inner IP */
	    struct in_addr iSmask;	/* Source netmask */
	    struct in_addr iDmask;	/* Destination netmask */
	    u_int16_t	   Sport; 	/* Source port, if applicable */
	    u_int16_t	   Dport;	/* Destination port, if applicable */
	    u_int8_t       Protocol;	/* Transport mode for which protocol */
	    u_int8_t 	   Sproto;	/* IPsec protocol */
	    u_int16_t	   Flags;
	    u_int32_t      Spi2;	/* Used in REPLACESPI... */
	    struct in_addr Dst2;	/* ...to specify which SPI is... */
	    u_int8_t       Sproto2;	/* ...replaced. */
	} Ena;

	/* For general use: (in)validate, delete (chain), reserve */
	struct 
	{
	    u_int32_t       Spi;
	    struct in_addr  Dst;
	    u_int8_t	    Sproto;
	} Gen;
    } Eu;
};

#define ENABLE_FLAG_REPLACE    	1	/* Replace existing flow with new */
#define ENABLE_FLAG_LOCAL      	2	/* Add routes for 0.0.0.0 */
#define ENABLE_FLAG_MODIFY     	4	/* Keep routing masks */

#define ENCAP_MSG_FIXED_LEN    	(2 * sizeof(u_int32_t))

#define NOTIFY_SOFT_EXPIRE     	0	/* Soft expiration of SA */
#define NOTIFY_HARD_EXPIRE     	1	/* Hard expiration of SA */
#define NOTIFY_REQUEST_SA      	2	/* Establish an SA */

#define NOTIFY_SATYPE_CONF	1	/* SA should do encryption */
#define NOTIFY_SATYPE_AUTH	2	/* SA should do authentication */
#define NOTIFY_SATYPE_TUNNEL	4	/* SA should use tunneling */

#define em_ena_spi	  Eu.Ena.Spi
#define em_ena_dst	  Eu.Ena.Dst
#define em_ena_isrc	  Eu.Ena.iSrc
#define em_ena_idst	  Eu.Ena.iDst
#define em_ena_ismask	  Eu.Ena.iSmask
#define em_ena_idmask	  Eu.Ena.iDmask
#define em_ena_sport	  Eu.Ena.Sport
#define em_ena_dport	  Eu.Ena.Dport
#define em_ena_protocol   Eu.Ena.Protocol
#define em_ena_sproto	  Eu.Ena.Sproto
#define em_ena_flags	  Eu.Ena.Flags

#define em_gen_spi        Eu.Gen.Spi
#define em_gen_dst        Eu.Gen.Dst
#define em_gen_sproto	  Eu.Gen.Sproto

#define em_not_type       Eu.Notify.Notification_Type
#define em_not_spi        Eu.Notify.Spi
#define em_not_dst        Eu.Notify.Dst
#define em_not_src	  Eu.Notify.Src
#define em_not_satype     Eu.Notify.Satype
#define em_not_userid     Eu.Notify.UserID
#define em_not_msgid      Eu.Notify.MsgID
#define em_not_sport      Eu.Notify.Sport
#define em_not_dport      Eu.Notify.Dport
#define em_not_protocol   Eu.Notify.Protocol
#define em_not_sproto	  Eu.Notify.Sproto

#define em_spi	          Eu.Xfm.Spi
#define em_dst	          Eu.Xfm.Dst
#define em_src	          Eu.Xfm.Src
#define em_osrc	          Eu.Xfm.oSrc
#define em_odst	          Eu.Xfm.oDst
#define em_alg	          Eu.Xfm.Alg
#define em_dat	          Eu.Xfm.Dat
#define em_first_use_hard Eu.Xfm.First_Use_Hard
#define em_first_use_soft Eu.Xfm.First_Use_Soft
#define em_expire_hard    Eu.Xfm.Expire_Hard
#define em_expire_soft    Eu.Xfm.Expire_Soft
#define em_bytes_hard     Eu.Xfm.Bytes_Hard
#define em_bytes_soft     Eu.Xfm.Bytes_Soft
#define em_packets_hard   Eu.Xfm.Packets_Hard
#define em_packets_soft   Eu.Xfm.Packets_Soft
#define em_ttl		  Eu.Xfm.TTL
#define em_sproto	  Eu.Xfm.Sproto
#define em_satype         Eu.Xfm.Satype

#define em_rel_spi	  Eu.Rel.Spi
#define em_rel_spi2	  Eu.Rel.Spi2
#define em_rel_dst	  Eu.Rel.Dst
#define em_rel_dst2	  Eu.Rel.Dst2
#define em_rel_sproto	  Eu.Rel.Sproto
#define em_rel_sproto2	  Eu.Rel.Sproto2

#define EMT_SETSPI	1		/* Set SPI properties */
#define EMT_GRPSPIS	2		/* Group SPIs */
#define EMT_DELSPI	3		/* delete an SPI */
#define EMT_DELSPICHAIN 4		/* delete an SPI chain starting from */
#define EMT_RESERVESPI  5		/* Give us an SPI */
#define EMT_ENABLESPI   6		/* Enable an SA */
#define EMT_DISABLESPI  7		/* Disable an SA */
#define EMT_NOTIFY      8		/* kernel->userland key mgmt not. */
#define EMT_REPLACESPI  10		/* Replace all uses of an SA */

/* Total packet lengths */
#define EMT_SETSPI_FLEN	      104
#define EMT_GRPSPIS_FLEN      26
#define EMT_GENLEN            17
#define EMT_DELSPI_FLEN       EMT_GENLEN
#define EMT_DELSPICHAIN_FLEN  EMT_GENLEN
#define EMT_RESERVESPI_FLEN   EMT_GENLEN
#define EMT_NOTIFY_FLEN       40
#define EMT_ENABLESPI_FLEN    49
#define EMT_DISABLESPI_FLEN   EMT_ENABLESPI_FLEN
#define EMT_REPLACESPI_FLEN   EMT_ENABLESPI_FLEN

#ifdef _KERNEL
extern struct ifaddr *encap_findgwifa(struct sockaddr *);
extern struct ifnet enc_softc;
#endif
