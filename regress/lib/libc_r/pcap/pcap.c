/*	$OpenBSD: pcap.c,v 1.3 2001/11/11 20:20:53 marc Exp $ */
/*
 *	Placed in the PUBLIC DOMAIN
 */

#include <pcap.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "test.h"

#define LOOPBACK_IF	"lo0"
#define SNAPLEN		96
#define NO_PROMISC	0
#define	PKTCNT		3

volatile int packet_count = 0;

void
packet_ignore(u_char *tag, const struct pcap_pkthdr *hdr, const u_char *data)
{
	packet_count += 1;
}

void *
pcap_thread(void *arg)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_t *handle;

	SET_NAME("pcap_thread");
	handle = pcap_open_live(LOOPBACK_IF, SNAPLEN, NO_PROMISC, 0, errbuf);
	if (!handle)
		PANIC("You may need to run this test as UID 0 (root)");
	ASSERT(pcap_loop(handle, PKTCNT, packet_ignore, 0) != -1);
	return 0;
}

void *
ping_thread(void *arg)
{
	SET_NAME("ping_thread");
	ASSERT(system("ping -c 3 127.0.0.1") == 0);
	sleep(2);
	ASSERT(packet_count == 3);
	SUCCEED;
}

int
main(int argc, char **argv)
{
	pthread_t pcap;
	pthread_t ping;

	CHECKr(pthread_create(&pcap, NULL, pcap_thread, NULL));
	sleep(1);
	CHECKr(pthread_create(&ping, NULL, ping_thread, NULL));
	while (1)
		;
	PANIC("while");
}
