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

struct servent * getservbyname_r __P((const char *, const char *, struct servent *, char *, int));
struct hostent * gethostbyname_r __P((const char *, struct hostent *, char *, int, int *));

int debug = 0;

static int test_serv()
{
	struct servent *serv;
	char answer[1024];

	if ((serv = getservbyname("telnet", "tcp")) != NULL)
		printf("getservbyname -> port %d\n", ntohs(serv->s_port));
	else
		printf("getservbyname -> NULL (bad)\n");

	if ((serv = getservbyname_r("telnet", "tcp", serv, answer, 1024))!=NULL)
		printf("getservbyname_r -> port %d\n", ntohs(serv->s_port));
	else
		printf("getservbyname_r -> NULL (bad)\n");
	return(OK);
}

static int test_host()
{
	struct hostent *host;
	struct in_addr addr;
	char answer[1024];
	int error;

	if ((host = gethostbyname("maze.mit.edu")) != NULL) {
		memcpy(&addr, host->h_addr, sizeof(addr));
		printf("gethostbyname -> %s\n", inet_ntoa(addr));
	} else {
		printf("gethostbyname -> NULL (bad)\n");
		host = (struct hostent *)answer;
	}

	if ((host = gethostbyname_r("maze.mit.edu", host, answer, 1024, &error))
		!= NULL) {
		memcpy(&addr, host->h_addr, sizeof(addr));
		printf("gethostbyname_r -> %s\n", inet_ntoa(addr));
	} else {
		printf("gethostbyname_r -> NULL (bad)\n");
	}
	return(OK);
}

static int test_localhost()
{
    struct hostent *host;

	if ((host = gethostbyname("127.0.0.1")) != NULL) {
		return(OK);
	}
	return(NOTOK);
}

/* ==========================================================================
 * usage();
 */
void usage(void)
{       
    printf("test_netdb [-d?]\n");
    errno = 0;
}

int
main(int argc, char **argv)
{

	/* Getopt variables. */
	extern int optind, opterr;
	extern char *optarg;
	char ch;

	while ((ch = getopt(argc, argv, "d?")) != (char)EOF) {
        switch (ch) {
        case 'd':
            debug++;
            break;
		case '?':
            usage();
            return(OK); 
        default:  
            usage();
            return(NOTOK);
        }
    }

	printf("test_netdb START\n");
	
	if (test_serv() || test_localhost() || test_host()) {
		printf("test_netdb FAILED\n");
		exit(1);
	}

	printf("test_netdb PASSED\n");
	exit(0);
}
