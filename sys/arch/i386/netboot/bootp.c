/*	$NetBSD: bootp.c,v 1.3 1994/10/27 04:21:08 cgd Exp $	*/

/*
 * bootp functions
 */

int BootpGetInfo(ipaddr_t *ip_servaddr, ipaddr_t *ip_myaddr, ipaddr_t *ip_gateway, char *file_name) {
  /* zero a packet */
  bzero(xxx);
  /* set dest to 255... */
  xxx = IP_BCASTADDR
  /* set up udp ports */
  /* send it */
  /* wait for reply or timeout */
  /* do exp backoff and retry if timeout */
  /* give up after a minute or so */
  /* return success or failure */
}

/* tbd - set up incoming packet handler to handle bootp replies??? */
