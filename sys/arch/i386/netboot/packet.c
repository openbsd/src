/*	$NetBSD: packet.c,v 1.4 1996/02/02 18:06:21 mycroft Exp $	*/

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 *
 * Packet allocation and deallocation routines.
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */

#include "proto.h"
#include "assert.h"
#include <sys/param.h>
#include "packet.h"

static packet_t *pool = (packet_t *)0;
static packet_t *last;

void
PktInit(void) {
  static packet_t s_pool[PKT_POOLSIZE];
  pool = s_pool;
  bzero((char *)pool, PKT_POOLSIZE * sizeof(packet_t));
  last = pool;
}

packet_t *
PktAlloc(u_long offset) {
  int i;

  for (i = 0; i < PKT_POOLSIZE; i++) {
    if (last->pkt_used == FALSE) {
      bzero((char *)last->pkt_data, PKT_DATASIZE);
      last->pkt_used = TRUE;
      last->pkt_len = 0;
      last->pkt_offset = last->pkt_data + offset;
#if 0
printf("PktAlloc: used %x\n", last);
#endif
      return last;
    }
    if (++last == &pool[PKT_POOLSIZE])
      last = pool;
  }
  printf("Pool out of free packets\n");
  exit(1);
  return 0; /* silence warnings */
}

void
PktRelease(packet_t *pkt) {
#if 0
printf("PktAlloc: freed %x\n", pkt);
#endif
    assert(pkt >= &pool[0]);
    assert(pkt < &pool[PKT_POOLSIZE]);
    (last = pkt)->pkt_used = FALSE;
}
