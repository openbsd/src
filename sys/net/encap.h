/*	$OpenBSD: encap.h,v 1.4 1997/06/25 07:53:20 provos Exp $	*/

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

/*
 * encap.h
 *
 * Declarations useful in the encapsulation code.
 */

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
	struct sockaddr Dfl;	/* SENT_DEFIF */
	struct			/* SENT_SA */
	{
	    struct sockaddr Src;
	    struct sockaddr Dst;
	} Sa;
#ifdef INET
	struct			/* SENT_SAIN */
	{
	    struct sockaddr_in Src;
	    struct sockaddr_in Dst;
	} Sin;
	struct			/* SENT_IP4 */
	{
	    struct in_addr Src;
	    struct in_addr Dst;
	    u_int16_t Sport;
	    u_int16_t Dport;
	    u_int8_t Proto;
	    u_int8_t Filler[3];
	} Sip4;
	struct			/* SENT_IPSP */
	{
	    struct in_addr Src;
	    struct in_addr Dst;
	    u_int32_t Spi;
	    u_int8_t Ifn;
	    u_int8_t Filler[3];
	} Sipsp;
	
#endif
    } Sen;
};

#define sen_data	Sen.Data
#define sen_dfl		Sen.Dfl
#define sen_sa_src	Sen.Sa.Src
#define sen_sa_dst	Sen.Sa.Dst
#ifdef INET
#define sen_sin_src	Sen.Sin.Src
#define sen_sin_dst	Sen.Sin.Dst
#define sen_ip_src	Sen.Sip4.Src
#define sen_ip_dst	Sen.Sip4.Dst
#define sen_ipsp_src	Sen.Sipsp.Src
#define sen_ipsp_dst	Sen.Sipsp.Dst
#define sen_ipsp_spi	Sen.Sipsp.Spi
#define sen_ipsp_ifn	Sen.Sipsp.Ifn
#define sen_proto	Sen.Sip4.Proto
#define sen_sport	Sen.Sip4.Sport
#define sen_dport	Sen.Sip4.Dport
#endif

/*
 * The "type" is really part of the address as far as the routing
 * system is concerned. By using only one bit in the type field
 * for each type, we sort-of make sure that different types of
 * encapsulation addresses won't be matched against the wrong type.
 * 
 */

#define SENT_DEFIF	0x0001		/* data is a default sockaddr for if */
#define SENT_SA		0x0002		/* data is two struct sockaddr */
#define SENT_SAIN	0x0004		/* data is two struct sockaddr_in */
#define SENT_IP4	0x0008		/* data is two struct in_addr */
#define SENT_IPSP	0x0010		/* data as in IP4 plus SPI and if# */

/*
 * SENT_HDRLEN is the length of the "header"
 * SENT_*_LEN are the lengths of various forms of sen_data
 * SENT_*_OFF are the offsets in the sen_data array of various fields
 */

#define SENT_HDRLEN	(2*sizeof(u_int8_t)+sizeof(u_int16_t))

#define SENT_DEFIF_LEN	(SENT_HDRLEN + sizeof (struct sockaddr_in))

#define SENT_IP4_SRCOFF	(0)
#define SENT_IP4_DSTOFF (sizeof (struct in_addr))
#define SENT_IP4_OPTOFF	(2*sizeof(struct in_addr)+2*sizeof(u_int16_t)+sizeof(u_int8_t)+3*sizeof(u_int8_t))

#define SENT_IP4_LEN	(SENT_HDRLEN + SENT_IP4_OPTOFF)

#define SENT_IPSP_LEN	(SENT_HDRLEN + 2 * sizeof (struct in_addr) + sizeof (u_int32_t) + 4)

/*
 * Options 0x00 and 01 are 1-byte options (no arguments).
 * The rest of the options are T-L-V fields, where the L includes
 * the T and L bytes; thus, the minimum length for an option with
 * no arguments is 2. An option of length less than 2 causes en EINVAL
 */
 

#define SENO_EOL	0x00		/* End of Options, or placeholder */
#define SENO_NOP	0x01		/* No Operation. Skip */
#define SENO_NAME	0x02		/* tunnel name, NUL-terminated */
#define SENO_SPI	0x03		/* Security Parameters Index */
#define SENO_IFN	0x04		/* Encap interface number */
#define SENO_IFIP4A	0x05		/* Encap interface IPv4 address */
#define SENO_IPSA	0x06		/* Encap interface generic sockaddr */

struct enc_softc
{
	struct ifnet enc_if;
};

/*
 * Tunnel descriptors are setup and torn down using a socket of the 
 * AF_ENCAP domain. The following defines the messages that can
 * be sent down that socket.
 */

#define EM_MAXRELSPIS	4		/* at most five chained xforms */
	

struct encap_msghdr
{
    u_int16_t	em_msglen;		/* message length */
    u_int8_t	em_version;		/* for future expansion */
    u_int8_t	em_type;		/* message type */
    union
    {
	struct
	{
	    struct in_addr Ia;
	    u_int8_t	Ifn;
	    u_int8_t  xxx[3];		/* makes life a lot easier */
	} Ifa;
	
	struct
	{
	    u_int32_t Spi;		/* SPI */
	    struct in_addr Dst;		/* Destination address */
	    u_int64_t Relative_Hard;	/* Expire relative to creation */
	    u_int64_t Relative_Soft;
	    u_int64_t First_Use_Hard;	/* Expire relative to first use */
	    u_int64_t First_Use_Soft;
	    u_int64_t Expire_Hard;	/* Expire at fixed point in time */
	    u_int64_t Expire_Soft;
	    u_int64_t Bytes_Hard;	/* Expire after bytes recved/sent */
	    u_int64_t Bytes_Soft;
	    u_int64_t Packets_Hard;	/* Expire after packets recved/sent */
	    u_int64_t Packets_Soft;
	    u_int32_t If;		/* enc i/f for input */
	    int32_t Alg;		/* Algorithm to use */
	    u_int8_t Dat[1];		/* Data */
	} Xfm;

	/*
 	 * For expiration notifications, the kernel fills in
	 * Notification_Type, Spi and Dst. No direct response is expected.
	 *
 	 * For SA Requests, the kernel fills in
	 * Notification_Type, MsgID, Spi, Seclevel, Dst (and optionally
	 * Protocol, Src, Sport, Dport and UserID).
 	 *
	 * The response should have the same values in all the fields
	 * and:
	 * Spi/Spi2/Spi3 will hold the SPIs for the three seclevels
	 * UserID can optionally hold the peer's UserID (if applicable)
	 */
	struct				/* kernel->userland notifications */
	{
	    u_int32_t Notification_Type;
#define  NOTIFY_SOFT_EXPIRE     0	/* Soft expiration of SA */
#define  NOTIFY_HARD_EXPIRE     1	/* Hard expiration of SA */
#define  NOTIFY_REQUEST_SA      2	/* Establish an SA */
	    u_int32_t MsgID;		/* Request ID */
	    u_int32_t Spi;		
	    u_int32_t Spi2;
	    u_int32_t Spi3;
	    u_int8_t Seclevel[3];	/* see netinet/in_pcb.h */
	    u_int8_t Protocol;		/* Transport mode for which protocol */
	    struct in_addr Dst;		/* Peer */
	    struct in_addr Src;		/* Might have our local address */
	    u_int16_t Sport;		/* Source port */
            u_int16_t Dport;		/* Destination port */
	    u_int8_t UserID[1];		/* Might be used to indicate user */
	} Notify;
	
	struct
	{
	    u_int32_t emr_spi;		/* SPI */
	    struct in_addr emr_dst;	/* Dest */
	    struct tdb *emr_tdb;	/* used internally! */
	    
	} Rel[EM_MAXRELSPIS];
    } Eu;
};

#define em_not_type      Eu.Notify.Notification_Type
#define em_not_spi       Eu.Notify.Spi
#define em_not_spi2      Eu.Notify.Spi2
#define em_not_spi3      Eu.Notify.Spi3
#define em_not_src       Eu.Notify.Src
#define em_not_dst       Eu.Notify.Dst
#define em_not_seclevel  Eu.Notify.Seclevel
#define em_not_userid    Eu.Notify.UserID
#define em_not_msgid     Eu.Notify.MsgID
#define em_not_sport     Eu.Notify.Sport
#define em_not_dport     Eu.Notify.Dport
#define em_not_protocol  Eu.Notify.Protocol

#define em_ifa	Eu.Ifa.Ia
#define em_ifn	Eu.Ifa.Ifn

#define em_spi	Eu.Xfm.Spi
#define em_dst	Eu.Xfm.Dst
#define em_if	Eu.Xfm.If
#define em_alg	Eu.Xfm.Alg
#define em_dat	Eu.Xfm.Dat
#define em_relative_hard  Eu.Xfm.Relative_Hard
#define em_relative_soft  Eu.Xfm.Relative_Soft
#define em_first_use_hard Eu.Xfm.First_Use_Hard
#define em_first_use_soft Eu.Xfm.First_Use_Soft
#define em_expire_hard    Eu.Xfm.Expire_Hard
#define em_expire_soft    Eu.Xfm.Expire_Soft
#define em_bytes_hard     Eu.Xfm.Bytes_Hard
#define em_bytes_soft     Eu.Xfm.Bytes_Soft
#define em_packets_hard   Eu.Xfm.Packets_Hard
#define em_packets_soft   Eu.Xfm.Packets_Soft

#define em_rel	Eu.Rel

#define EMT_IFADDR	1		/* set enc if addr */
#define EMT_SETSPI	2		/* Set SPI properties */
#define EMT_GRPSPIS	3		/* Group SPIs (output order)  */
#define EMT_DELSPI	4		/* delete an SPI */
#define EMT_DELSPICHAIN 5		/* delete an SPI chain starting from */
#define EMT_RESERVESPI  6		/* Give us an SPI */
#define EMT_ENABLESPI   7		/* Enable an SA */
#define EMT_DISABLESPI  8		/* Disable an SA */
#define EMT_NOTIFY      9		/* kernel->userland key mgmt not. */

#define EM_MINLEN	8		/* count!!! */
#define EMT_IFADDR_LEN	12
#define EMT_SETSPI_FLEN	20
#define EMT_GRPSPIS_FLEN 4
#define EMT_DELSPI_FLEN 20
#define EMT_DELSPICHAIN_FLEN 20

#ifdef _KERNEL
extern struct ifaddr *encap_findgwifa(struct sockaddr *);
extern struct enc_softc *enc_softc;
extern int32_t nencap;
#endif
