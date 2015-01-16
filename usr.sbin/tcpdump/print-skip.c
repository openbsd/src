/*	$OpenBSD: print-skip.c,v 1.5 2015/01/16 06:40:21 deraadt Exp $	*/

/*
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * All rights reserved.
 * 
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.

 * IN NO EVENT SHALL SUN MICROSYSTEMS, INC. BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
 * SUN MICROSYSTEMS, INC. HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.

 * SUN MICROSYSTEMS, INC. SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
 * THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND SUN
 * MICROSYSTEMS, INC. HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT,
 * UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/

#include <sys/time.h> 
#include <sys/types.h>
 
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "interface.h"
#include "addrtoname.h"

#define MAX_ALGS		(256)

extern int skipflag;

const int       skip_max_algs = MAX_ALGS;

char            *old_skip_crypt_algs[MAX_ALGS] = {
                                        "none",         /* 0 */
                                        "des_cbc",      /* 1 */
                                        "rc2_cbc",      /* 2 */
                                        "rc4(40bit)",   /* 3 */
                                        "rc4(128bit)",  /* 4 */
                                        "des_ede-2",    /* 5 */
                                        "des_ede-3",    /* 6 */
                                        "idea",         /* 7 */
                                        "",             /* 8 */
                                        "",             /* 9 */
                                        "simplecrypt"   /* 10 */
                };


char *
skip_alg_to_name(char *table[], int alg)
{
        if (alg > skip_max_algs) {
                return ("<invalid>");
        }
        if (alg < 0) {
                return ("<invalid>");
        }
        if (table[alg] == NULL) {
                return ("<unknown>");
        }
        if (strlen(table[alg]) == 0) {
                return ("<unknown>");
        }
        return (table[alg]);
}

/*
 * This is what an OLD skip encrypted-authenticated packet looks like:
 *
 *
 *                 0       1      2      3
 *            ---------------------------------
 *            |                               |
 *            /      Clear IP Header          /  
 *            |                               |     IP protocol = IPSP
 *            ---------------------------------
 *            |                               |
 *            |         IPSP header           |  
 *            |                               |
 *            ---------------------------------
 *            |                               |
 *            /    Protected  IPSP Payload    /
 *            /                               /
 *            |                               |
 *            ---------------------------------
 *
 *
 * The format of the IPSP header for encrypted-encapsulated mode is shown below. * The fields are transmitted from left to right.
 *
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  | Ver.  |E|A|C|S|B|R|               zero                        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |    Kij alg.   |   Kp alg.     |        reserved               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |    Optional boxid field                                       |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |    Kp encrypted in Kij...          (typically 8-16 bytes)
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |    Message Indicator (e.g IV)...   (typically 8-16 bytes)
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |    Protected IPSP Payload...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *
 *   Field values:
 *   Ver.: protocol version
 *   E:    1 if packet is encrypted, 0 otherwise
 *   A:    1 if packet is authenticated, 0 otherwise
 *   C:    1 if packet is compressed before encryption, 0 otherwise
 *   S:    1 if packet is sequenced, 0 otherwise
 *   B:    1 if packet is tunneled (header contains boxid), 0 otherwise
 *   R:    reserved (should be 0 until specified)
 *
 */
/*
 * per-algorithm encrytped key sizes...
 */
unsigned char   old_skip_ekp_sizes[MAX_ALGS] = {
                          8,    /* plaintext */
                          8,    /* DES */
                          8,    /* RC2 */
                          8,    /* RC4 (40 bit)  */
                         16,    /* RC4 (128 bit) */
                         16,    /* 3DES 2 */
                         24,    /* 3DES 3 */
                         16,    /* IDEA */
                          0,    /* */
                          0,    /* */
                          8,    /* simplecrypt */
                };
/*
 * per-algorithm message indicator sizes...
 */
unsigned char   old_skip_mid_sizes[MAX_ALGS] = {
                          8,    /* plaintext */
                          8,    /* DES */
                          8,    /* RC2 */
                          8,    /* RC4 40 bit */
                          8,    /* RC4 128 bit */
                          8,    /* 3DES 2 */
                          8,    /* 3DES 3 */
                          8,    /* IDEA */
                          0,    /* */
                          0,    /* */
                          8,    /* simplecrypt */
                };

void skip_print_old(register const u_char *bp, register int length, 
							const u_char *bp2)
{
	struct ip *ip;
	const u_char *end;
	u_char *p;
	unsigned char	kij_alg, kp_alg, *c;
	unsigned short i;
	unsigned short len;
	int boxid;
	int node;

	ip=(struct ip *)bp2;
	p=(u_char *)bp;
	end=bp+length;
	printf("SKIP: *** OLD SKIP ***\n");
	printf("OSKIP: %s>%s:%d",ipaddr_string(&ip->ip_src),
			       ipaddr_string(&ip->ip_dst),length);
	if (!skipflag)
		return;
	printf("\nOSKIP: SAID byte 1= 0x%02x\n",*p);
	printf("OSKIP:    xxxx .... = version %d\n", (int) (*p & 0xf0) >> 4);
        if (*p & 0x08) {
                printf("OSKIP:    .... 1... = encrypted\n");
        } else {
                printf("OSKIP:    .... 0... = not encrypted\n");
        }
 
        if (*p & 0x04) {
                printf("OSKIP:    .... .1.. = authenticated\n");
        } else {
                printf("OSKIP:    .... .0.. = not authenticated\n");
        }
 
        if (*p & 0x02) {
                printf("OSKIP:    .... ..1. = compressed\n");
        } else {
                printf("OSKIP:    .... ..0. = not compressed\n");
        }
 
        if (*p & 0x01) {
                printf("OSKIP:    .... ...1 = sequenced\n");
        } else {
                printf("OSKIP:    .... ...0 = not sequenced\n");
        }
                           
        p++;

  	printf("OSKIP: SAID byte 2 = 0x%02x\n", *p);
 
        if (*p & 0x80) {
		node=1;
                printf("OSKIP:    1... .... = Node ID present\n");
        } else {
		node=0;
                printf("OSKIP:    0... .... = no Node ID present\n");
        }
 
        if (*p & 0x40) {
                printf("OSKIP:    .1.. .... = <reserved should be zero>\n");
        } else {
                printf("OSKIP:    .0.. .... = <reserved should be zero>\n");
        }
 
        if (*p & 0x20) {
                printf("OSKIP:    ..1. .... = <reserved should be zero>\n");
        } else {
                printf("OSKIP:    ..0. .... = <reserved should be zero>\n");
        }
 
        if (*p & 0x10) {
                printf("OSKIP:    ...1 .... = <reserved should be zero>\n");
        } else {
                printf("OSKIP:    ...0 .... = <reserved should be zero>\n");
        }
	  p++;
        printf("OSKIP: SAID byte 3 = 0x%02x\n", *p);
 
        p++;     
        printf("OSKIP: SAID byte 4 = 0x%02x\n", *p);
 
        p++;
 
        kij_alg = *p;
        printf("OSKIP: Kij alg (key encryption algorithm) = 0x%02x (%s)\n",
		    kij_alg, skip_alg_to_name(old_skip_crypt_algs,kij_alg));
 
        p++;
 
        kp_alg = *p;
        printf("OSKIP: Kp alg (traffic encryption algorithm) = 0x%02x (%s)\n",
		    kp_alg, skip_alg_to_name(old_skip_crypt_algs,kp_alg));
 
        p++;
 
        /*
         * the skip reserved field
         */
        printf("OSKIP: reserved byte 1 = 0x%02x\n", *p++);
        printf("OSKIP: reserved byte 2 = 0x%02x\n", *p++);

        if (node) {
        /*
         * boxid field
         */
	    if ((end - p) < sizeof(boxid)) {
		    return;
	    }
	    c = (unsigned char *) &boxid;
	    *c++ = *p++;
	    *c++ = *p++;
	    *c++ = *p++;
	    *c++ = *p++;

	    printf("OSKIP: Node ID = 0x%08x\n", ntohl(boxid));
        }

        /*
         * encrypted kp (ekp) field
         */

        /*
         * do this with a for-loop to avoid alignment problems and the
         * overhead of calling bcopy()
         */
        len     = old_skip_ekp_sizes[kp_alg];
        if ((unsigned short) (end - p) < len) {
                return;
        }

        printf("OSKIP: encrypted Kp:  ");
        for (i = 0; i < len; i++) {
                printf("%02x ", (unsigned char) *p++);
        }
        printf("\n");

        /*
         * message indicator (mid) field
         */
        len     = old_skip_mid_sizes[kp_alg];
        if ((unsigned short) (end - p) < len) {
                return;
        }
        printf("OSKIP: message indicator field:  ");
        for (i = 0; i < len; i++) {
                printf("%02x ", (unsigned char) *p++);
        }
        printf("\n");
}



/* 
 * The following part is (c) by G. Caronni  -- 29.11.95 
 *
 * This code is in the public domain; do with it what you wish.
 *
 * NO WARRANTY, NO SUPPORT, NO NOTHING!
 */


/*
 * This is what a NEW skip encrypted-authenticated packet looks like:
 *
 *
 *                 0       1      2      3
 *            ---------------------------------
 *            |                               |
 *            /      Clear IP Header          /  
 *            |                               |     IP protocol = SKIP
 *            ---------------------------------
 *            |                               |
 *            |         SKIP header           |  
 *            |                               |
 *            ---------------------------------
 *            |                               |
 *            |    Auth Header & payload      |  
 *            |                               |
 *            ---------------------------------
 *            |                               |
 *            |    ESP header and SPI         |  
 *            |                               |
 *            ---------------------------------
 *            |                               |
 *            /    Protected  ESP Payload     /
 *            |                               |
 *            ---------------------------------
 *
 *
 * The format of the SKIP header for encrypted-encapsulated mode is shown below. * The fields are transmitted from left to right.
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |  Ver  | Rsvd  | Source NSID   | Dest NSID     | NEXT HEADER   |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                    Counter n                                  |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |    Kij Alg    |   Crypt Alg   |  MAC Alg      |  Comp Alg     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |    Kp encrypted in Kijn...          (typically 8-16 bytes)
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |    Source Master Key-ID  (If Source NSID is non-zero)
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |    Destination Master Key-ID (If Dest NSID is non-zero)
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */





/*
 * per name space key ID sizes...
 */
unsigned char   skip_nsid_sizes[MAX_ALGS] = {
                          0,    /* 0  none */
                          4,    /* 1  IP v4 Address Space */
                          4,    /* 2  POSIX/XOPEN User Ids */
                         16,    /* 3  IPv6 Address Space */
                         16,    /* 4  MD5 of DNS Names */
                         16,    /* 5  MD5 of ISO ASN.1 DN encoding */
                         16,    /* 6  MD5 of US Social Security number */
                          6,    /* 7  802.x MAC Address */
                         16,    /* 8  MD5 of public Value */
                         16,    /* 9  MD5 of RFC822 Mailbox Address */
                         16,    /* 10 MD5 of Bank Account # */
                         16,    /* 11 MD5 of NIS Name */
                };


/*
 * per Kp algorithm encrypted Kp sizes... (Kij alg does not matter for now)
 */
unsigned char   skip_ekp_sizes[MAX_ALGS] = {
                          0,    /* 0 plaintext */
                          8,    /* 1 DES_CBC */
                          24,   /* 2 3 key triple DES-EDE-CBC */
                          0,    /* 3  */
                          0,    /* 4  */
                          0,    /* 5  */
                          0,    /* 6  */
                          0,    /* 7  */
                          0,    /* 8  */
                          0,    /* 9  */                        /* 10 .. 249 */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			  8,    /* 250 RC4-40 */                          
			  16,   /* 251 RC4-128 */                          
			  8,    /* 252 simple crypt */                          
			  16,   /* 253 IDEA */                          
			  0,    /* 254 */                          
			  0     /* 255 */                          
                };


/*
 * per-algorithm NSID names ...
 */
char            *skip_nsid_names[MAX_ALGS] = {
                                        "none",             /* 0 */
                                        "IPv4",             /* 1 */
                                        "Posix/Xopen UID",  /* 2 */
                                        "IPv6",             /* 3 */
                                        "MD5 DNS",          /* 4 */
                                        "MD5 ASN.1 DN",     /* 5 */
                                        "MD5 U.S. Soc. #",  /* 6 */
                                        "802.x MAC",        /* 7 */
                                        "MD5 DH Public Key",/* 8 */
                                        "MD5 RFC822 Mail",  /* 9 */
                                        "MD5 Bank Account", /* 10 */
                                        "MD5 NIS Name",     /* 11 */
                };


/*
 * per-algorithm Kij alg names ...
 */
char            *skip_kij_names[MAX_ALGS] = {
                                        "none",             /* 0 */
                                        "DES-CBC",          /* 1 */
                                        "3DES3-EDE-CBC",    /* 2 */
                                        "IDEA-CBC",         /* 3 */
		};


/* for padding of ekp */

char            skip_kij_sizes[MAX_ALGS] = {
                                        0, /* 0 none */
                                        8, /* 1  des-cbc */
                                        8, /* 2 3des3-ede-cbc */
                                        8, /* 3 idea-cbc */
		};


/*
 * per-algorithm Crypt alg names ...
 */
char            *skip_crypt_names[MAX_ALGS] = {
                                        "none",                      /* 0 */
                                        "DES-CBC",                   /* 1 */
                                        "3 key DES-EDE-CBC",         /* 2 */
                                        "",                          /* 3 */
                                        "",                          /* 4 */
                                        "",                          /* 5 */
                                        "",                          /* 6 */
                                        "",                          /* 7 */
                                        "",                          /* 8 */
					"",   /* 9 */        /* 10 .. 249 */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
					"RC4-40",                    /* 250 */ 
					"RC4-128",                   /* 251 */
					"simple crypt",              /* 252 */ 
					"IDEA CBC",                  /* 253 */
					"",                          /* 254 */ 
					""                           /* 255 */
                };


/*
 * per-algorithm Auth alg names ...
 */
char            *skip_auth_names[MAX_ALGS] = {
                                        "none",        /* 0 */
                                        "keyed MD5",   /* 1 */
                                        "DES-CBC MAC", /* 2 */
                                        "Keyed SHA",   /* 3 */
                };


char            skip_auth_sizes[MAX_ALGS] = {
                                        0, 		/* 0 none */
                                       16,	 	/* 1 keyed MD5 */
                                        8, 		/* 2 DES-CBC MAC */
                                       20, 		/* 3 Keyed SHA */
                };


/*
 * per-algorithm Crypt alg IV sizes ...
 */
char            skip_crypt_sizes[MAX_ALGS] = {
                                        0, /* 0 none */
                                        8, /* 1 DES-CBC */
                                        8, /* 2 3key DES-EDE-CBC */
                                        0, /* 3 */
                                        0, /* 4 */
                                        0, /* 5 */
                                        0, /* 6 */
                                        0, /* 7 */
                                        0, /* 8 */
					0, /* 9 */        /* 10 .. 249 */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
					8, /* 250 RC4-40 */ 
				        8, /* 251 RC4-128 */
					8, /* 252 simple crypt */ 
					8, /* 253 IDEA CBC */
					0, /* 254 */ 
					0  /* 255 */
                };


#ifndef IPPROTO_ESP
#define IPPROTO_ESP 50
#endif
#ifndef IPPROTO_AH
#define IPPROTO_AH 51
#endif
#ifndef IPPROTO_SKIP
#define IPPROTO_SKIP 57
#endif
#ifndef IPPROTO_OSKIP
#define IPPROTO_OSKIP 79
#endif

static int expected_auth_size=0;
static int expected_iv_size=0;

char *skip_protocol_name(int p)
{
    switch(p) {
	case IPPROTO_IP:    return "IP";
	case IPPROTO_ICMP:  return "ICMP";
	case IPPROTO_IGMP:  return "IGMP";
	case IPPROTO_ENCAP: return "ENCAP";
	case IPPROTO_TCP:   return "TCP";
	case IPPROTO_EGP:   return "EGP";
	case IPPROTO_UDP:   return "UDP";
	case IPPROTO_ESP:   return "ESP";
	case IPPROTO_AH:    return "AH";
	case IPPROTO_SKIP:  return "SKIP";
	case IPPROTO_ND:    return "ND";
	case IPPROTO_OSKIP: return "OLD-SKIP";
	case IPPROTO_RAW:   return "RAW IP";
	default:            return "<unknown>";
    }
}

void skip_print_next(u_char nxt, const u_char *p, int len, const u_char *bp2)
{
    switch(nxt) {
	case IPPROTO_IP:   ip_print(p,len); break;
	case IPPROTO_ICMP: icmp_print(p,bp2); break;
	case IPPROTO_TCP:  tcp_print(p,len,bp2); break;
	case IPPROTO_UDP:  udp_print(p,len,bp2); break;
	case IPPROTO_ESP:  esp_print(p,len,bp2); break;
	case IPPROTO_AH:   ah_print(p,len,bp2); break;
	case IPPROTO_SKIP: skip_print(p,len,bp2); break;
	default: break;
    }
}

void skip_print(register const u_char *bp, register int length, 
							const u_char *bp2)
{
	struct ip *ip;
	const u_char *end;
	const u_char *p;
	unsigned char	kij_alg, crypt_alg, auth_alg, snsid, dnsid, nxt;
	unsigned short i;
	unsigned short len;
	u_int n;
	time_t full_n;

	ip=(struct ip *)bp2;
	p=bp;
	end=bp+length<snapend?bp+length:snapend;

	printf("%s>%s:%d SKIP",ipaddr_string(&ip->ip_src),
			       ipaddr_string(&ip->ip_dst),length);
	if (!skipflag)
		return;


        if ((unsigned short) (end - p) < 4) {
	    printf("[SKIP|] (truncated)\n");
	    return;
        }

	printf("\nSKIP: version\t\t\t%d\n", (int) (*p & 0xf0) >> 4);
	if (*p & 0xf) 
		printf("SKIP: version byte \t\treserved,\tis now 0x%x\n", 
							(int) (*p & 0xf));
        p++;

        snsid = *p;
        printf("SKIP: Source NSID\t\t0x%02x\t\t%s\n",
		    snsid, skip_alg_to_name(skip_nsid_names,snsid));
        p++;

        dnsid = *p;
        printf("SKIP: Destination NSID\t\t0x%02x\t\t%s\n",
		    dnsid, skip_alg_to_name(skip_nsid_names,dnsid));
        p++;

        nxt = *p;
        printf("SKIP: Next Protocol Field\t0x%02x\t\t%s\n", nxt, 
						    skip_protocol_name(nxt));
 
        p++;

        if ((unsigned short) (end - p) < 4) {
	    printf("[SKIP|] (truncated)\n");
	    return;
        }

	n=*p++<<24; 
	n+=*p++<<16;
	n+=*p++<<8;
	n+=*p;
	full_n=(((365*25+6)*24)+n)*3600;
        printf("SKIP: Counter n Field\t\t0x%08x\t%s", n, 
						asctime(gmtime(&full_n)));
        p++;

        if ((unsigned short) (end - p) < 4) {
	    printf("[SKIP|] (truncated)\n");
	    return;
        }

        kij_alg = *p;
        printf("SKIP: Kij alg (key encryption)\t0x%02x\t\t%s\n",
		    kij_alg, skip_alg_to_name(skip_kij_names,kij_alg));
        p++;
 
        crypt_alg = *p;
	expected_iv_size=skip_crypt_sizes[crypt_alg];
        printf("SKIP: Crypt Alg\t\t\t0x%02x\t\t%s\n",
		    crypt_alg, skip_alg_to_name(skip_crypt_names,crypt_alg));
        p++;
 
        auth_alg = *p;
	expected_auth_size=skip_auth_sizes[auth_alg];
        printf("SKIP: Auth Alg\t\t\t0x%02x\t\t%s\n",
		    auth_alg, skip_alg_to_name(skip_auth_names,auth_alg));
        p++;
 
        if (*p) printf("SKIP: compression\t\treserved,\tis now0x%02x\n",
								(int) *p++);
        else p++;

        /*
         * encrypted kp (ekp) field
         */

	if (kij_alg==0 && (crypt_alg || auth_alg)) {
	    printf("Warning: Kij Alg. undefined, but Auth. or Crypt. used!");
	    printf("Warning: Assuming empty Kp\n");
	    crypt_alg=auth_alg=0;
	}
        /*
         * do this with a for-loop to avoid alignment problems and the
         * overhead of calling bcopy()
         */
        len = skip_ekp_sizes[crypt_alg];
        len = len>(int)skip_auth_sizes[auth_alg]?len:skip_auth_sizes[auth_alg];
        if (len && skip_kij_sizes[kij_alg] && len % skip_kij_sizes[kij_alg]) {
	    len += skip_kij_sizes[kij_alg] - (len%skip_kij_sizes[kij_alg]);
	}
        if ((unsigned short) (end - p) < len) {
	    printf("[SKIP|] (truncated)\n");
	    return;
        }

        printf("SKIP: Encrypted Kp\t\t");
        for (i = 0; i < len; i++) {
                printf("%02x ", (unsigned char) *p++);
        }
        printf("\n");


        if (snsid) {
        /*
         * Source Master Key-ID field
         */
	    if ((end - p) < skip_nsid_sizes[snsid]) {
		printf("[SKIP|] (truncated)\n");
		return;
	    }
	    printf("SKIP: Source Master Key-ID\t");
            if (snsid==1) {
		printf("%s",ipaddr_string(p));
		p+=skip_nsid_sizes[snsid];
	    } else {
		for (i = 0; i < skip_nsid_sizes[snsid]; i++) {
		    printf("%02x ", (unsigned char) *p++);
		}
	    }
	    printf("\n");
        }

        if (dnsid) {
        /*
         * Destination Master Key-ID field
         */
	    if ((end - p) < skip_nsid_sizes[dnsid]) {
		printf("[SKIP|] (truncated)\n");
		return;
	    }
	    printf("SKIP: Dest. Master Key-ID\t");
            if (dnsid==1) {
		printf("%s",ipaddr_string(p));
		p+=skip_nsid_sizes[dnsid];
	    } else {
		for (i = 0; i < skip_nsid_sizes[dnsid]; i++) {
		    printf("%02x ", (unsigned char) *p++);
		}
	    }
	    printf("\n");
        }
        if (p<end) skip_print_next(nxt,p,end-p,bp2);
        else printf("(truncated)\n");
}



void ah_print(register const u_char *bp, register int length, 
							const u_char *bp2)
{
	struct ip *ip;
	const u_char *end;
	const u_char *p;
	u_char	nxt;
	int len, i;
	u_int spi;

	ip=(struct ip *)bp2;
	p=bp;
	end=bp+length<snapend?bp+length:snapend;


	printf("SKIP-AH: %s>%s:%d",ipaddr_string(&ip->ip_src),
			       ipaddr_string(&ip->ip_dst),length);
	if (!skipflag)
		return;

	if (end-p <4) {
            printf("[SKIP-AH|]\n");
	    return;
	}

        nxt = *p;
        printf("\nSKIP-AH: Next Protocol Field\t0x%02x\t\t%s\n", nxt, 
						    skip_protocol_name(nxt));
        p++;

	len= 4 * (int) *p;
	printf("SKIP-AH: length\t\t\t%d\n", len);

        p++;

        if (*p) printf("SKIP-AH: byte 3\t\t\treserved,\tis now0x%02x\n",
								(int) *p++);
        else p++;
        if (*p) printf("SKIP-AH: byte 4\t\t\treserved,\tis now0x%02x\n",
								(int) *p++);
        else p++;

	if (end-p <4) {
            printf("[SKIP-AH|]\n");
	    return;
	}

	spi=*p++<<24; 
	spi+=*p++<<16;
	spi+=*p++<<8;
	spi+=*p;
        printf("SKIP-AH: SPI\t\t\t0x%08x\t", spi );
	if (spi==0) {
	    printf("NO association\n");
	} else if (spi==1) {
	    printf("SKIP association\n");
	    if (expected_auth_size) {
		if (expected_auth_size != len) {
		    printf("Warning: Length does not match SKIP Auth Alg!\n");
		}
		expected_auth_size=0;
	    }
	} else if (spi<256) {
	    printf("UNKNOWN association\n");
	} else {
	    printf("DYNAMIC association\n");
        }

        p++;
 

        /*
         * authentication data
         */

        if ((unsigned short) (end - p) < len) {
	    printf("[SKIP-AH|] (truncated)\n");
	    return;
        }

        printf("SKIP-AH: Authentication Data\t");
        for (i = 0; i < len; i++) {
                printf("%02x ", (unsigned char) *p++);
		if (i<len-1 && (i+1)%16==0) printf("\n\t\t\t\t");
        }
        printf("\n");

        if (p<end) skip_print_next(nxt,p,end-p,bp2);
        else printf("(truncated)\n");
}


void esp_print(register const u_char *bp, register int length, 
							const u_char *bp2)
{
	struct ip *ip;
	const u_char *end;
	const u_char *p;
	int len, i;
	u_int spi;

	ip=(struct ip *)bp2;
	p=bp;
	end=bp+length<snapend?bp+length:snapend;


	printf("SKIP-ESP: %s>%s:%d",ipaddr_string(&ip->ip_src),
			       ipaddr_string(&ip->ip_dst),length);
	if (!skipflag)
		return;

	if (end-p <4) {
            printf("[SKIP-ESP|]\n");
	}

	spi=*p++<<24; 
	spi+=*p++<<16;
	spi+=*p++<<8;
	spi+=*p;
        printf("\nSKIP-ESP: SPI\t\t\t0x%08x\t", spi );
	if (spi==0) {
	    printf("NO association\n");
	    len=0;
	} else if (spi==1) {
	    printf("SKIP association\n");
	    len=expected_iv_size;
	    if (!expected_iv_size) {
		printf("Warning: IV size not defined by SKIP Crypt Alg!\n");
	    } else expected_iv_size=0;
	} else if (spi<256) {
	    printf("UNKNOWN association\n");
	    len=0;
	} else {
	    printf("DYNAMIC association\n");
	    len=0;
        }

        p++;
 
        /*
         * IV data
         */

        if ((unsigned short) (end - p) < len) {
	    printf("[SKIP-ESP|] (truncated)\n");
	    return;
        }

        printf("SKIP-ESP: Initialization Vector\t");
	if (len) {
	    for (i = 0; i < len; i++) {
		    printf("%02x ", (unsigned char) *p++);
		    if (i<len-1 && (i+1)%16==0) printf("\n\t\t\t\t");
	    }
        } else {
	    printf("UNDEFINED (unknown algorithm)");
	}
	printf("\n");

	/* no further analysis is possible without decrypting */
}

