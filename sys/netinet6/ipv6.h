/*
%%% copyright-nrl-95
This software is Copyright 1995-1998 by Randall Atkinson, Ronald Lee,
Daniel McDonald, Bao Phan, and Chris Winters. All Rights Reserved. All
rights under this copyright have been assigned to the US Naval Research
Laboratory (NRL). The NRL Copyright Notice and License Agreement Version
1.1 (January 17, 1995) applies to this software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

*/
#ifndef _NETINET6_IPV6_H
#define _NETINET6_IPV6_H 1

#define IPV6VERSION 6

/*
 * Header structures.
 */

struct ipv6
{
  uint32_t ipv6_versfl;      /* Version and flow label word. */
 
  uint16_t ipv6_length;      /* Datagram length (not including the length
			       of this header). */
  uint8_t ipv6_nexthdr;      /* Next header type. */
  uint8_t ipv6_hoplimit;     /* Hop limit. */
 
  struct in6_addr ipv6_src; /* Source address. */
  struct in6_addr ipv6_dst; /* Destination address. */
};

#if __linux__
#include <endian.h>
#else /* __linux__ */
#include <machine/endian.h>
#endif /* __linux__ */

struct ipv6hdr {
#if BYTE_ORDER == LITTLE_ENDIAN
  uint8_t ipv6_priority:4; /* going away? */
  uint8_t ipv6_version:4;
  uint32_t ipv6_flowid:24;
#elif BYTE_ORDER == BIG_ENDIAN
  uint32_t ipv6_flowid:24;
  uint8_t ipv6_priority:4; /* going away? */
  uint8_t ipv6_version:4;
#else
#error "Don't know what endian to use."
#endif
  uint16_t ipv6_len;
  uint8_t ipv6_nextheader;
  uint8_t ipv6_hoplimit;
  struct in6_addr ipv6_src;   /* source address */
  struct in6_addr ipv6_dst;   /* destination address */
};

/*
 * Macros and defines for header fields, and values thereof.
 * Assume things are in host order for these three macros.
 */

#define IPV6_VERSION(h) ((h)->ipv6_versfl >> 28)
#define IPV6_PRIORITY(h)  (((h)->ipv6_versfl & 0x0f000000) >> 24)
#define IPV6_FLOWID(h)  ((h)->ipv6_versfl & 0x00ffffff)

#define MAXHOPLIMIT 64
#define IPV6_MINMTU 576

/*
 * Other IPv6 header definitions.
 */

/* Fragmentation header & macros for it.  NOTE:  Host order assumption. */

struct ipv6_fraghdr
{
  uint8_t frag_nexthdr;      /* Next header type. */
  uint8_t frag_reserved;
  uint16_t frag_bitsoffset;  /* More bit and fragment offset. */
  uint32_t frag_id;          /* Fragment identifier. */
};

#define FRAG_MOREMASK 0x1
#define FRAG_OFFMASK 0xFFF8
#define FRAG_MORE_BIT(fh)       ((fh)->frag_bitsoffset & FRAG_MOREMASK)
#define FRAG_OFFSET(fh)         ((fh)->frag_bitsoffset & FRAG_OFFMASK)

/* Source routing header.  Host order assumption for macros. */

struct ipv6_srcroute0
{
  uint8_t i6sr_nexthdr;    /* Next header type. */
  uint8_t i6sr_len;        /* RH len in 8-byte addrs, !incl this structure */
  uint8_t i6sr_type;       /* Routing type, should be 0 */
  uint8_t i6sr_left;       /* Segments left */
  uint32_t i6sr_reserved;  /* 8 bits of reserved padding. */
};

#define I6SR_BITMASK(i6sr)      ((i6sr)->i6sr_reserved & 0xffffff)

/* Options header.  For "ignoreable" options. */

struct ipv6_opthdr
{
  uint8_t oh_nexthdr;        /* Next header type. */
  uint8_t oh_extlen;         /* Header extension length. */
  uint8_t oh_data[6];        /* Option data, may be reserved for
			       alignment purposes. */
};

#define OPT_PAD1 0
#define OPT_PADN 1
#define OPT_JUMBO 194

struct ipv6_option
{
  uint8_t opt_type;      /* Option type. */
  uint8_t opt_datalen;   /* Option data length. */
  uint8_t opt_data[1];   /* Option data. */
};
#endif /* _NETINET6_IPV6_H */
