/*	$NetBSD: packet.h,v 1.3 1994/10/27 04:21:22 cgd Exp $	*/

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 *
 * Packet layout definitions
 */

/* implementation constants */
#define	PKT_POOLSIZE	5
#define	PKT_DATASIZE	1514

/*
 * Structure of a packet.
 * Each packet can hold exactly one ethernet message.
 */
typedef struct {
  u_short pkt_used;			/* whether this packet it used */
  u_short pkt_len;			/* length of data  */
  u_char *pkt_offset;			/* current offset in data */
  u_char pkt_data[PKT_DATASIZE];	/* packet data */
} packet_t;

void PktInit(void);
packet_t *PktAlloc(u_long);
void PktRelease(packet_t *);
