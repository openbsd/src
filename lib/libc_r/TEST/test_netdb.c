/*	$OpenBSD: test_netdb.c,v 1.4 2000/01/06 06:54:43 d Exp $	*/
/*
 * Copyright (c) 1995 by Greg Hudson, ghudson@.mit.edu
 *
 * Test netdb calls.
 */

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "test.h"

static void test_serv()
{
	struct servent *serv;

	CHECKhn(serv = getservbyname("telnet", "tcp"));
	printf("getservbyname -> port %d\n", ntohs(serv->s_port));
}

static void test_host()
{
	struct hostent *host;
	struct in_addr addr;

	CHECKhn(host = gethostbyname("localhost"));
	memcpy(&addr, host->h_addr, sizeof(addr));
	printf("gethostbyname -> %s\n", inet_ntoa(addr));
}

static void test_localhost()
{
	struct hostent *host;

	CHECKhn(host = gethostbyname("127.0.0.1"));
}

int
main(int argc, char **argv)
{
	test_serv();
	test_localhost();
	test_host();

	SUCCEED;
}
