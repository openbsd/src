/* ==== test_netdb.c =========================================================
 * Copyright (c) 1995 by Greg Hudson, ghudson@.mit.edu
 *
 * Description : Test netdb calls.
 *
 *  1.00 95/01/05 ghudson
 *      -Started coding this file.
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
