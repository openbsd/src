/* $OpenBSD: ipsecadm.c,v 1.4 1997/06/20 06:14:38 provos Exp $ */
/*
 * The author of this code is John Ioannidis, ji@tla.org,
 * 	(except when noted otherwise).
 *
 * This code was written for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis, kermit@forthnet.gr.  Additional code written by
 * Niels Provos in Germany.
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

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netns/ns.h>
#include <netiso/iso.h>
#include <netccitt/x25.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include "net/encap.h"
#include "netinet/ip_ipsp.h"
#include "netinet/ip_esp.h"

typedef struct {
	char *name;
	int    (*func) (int, char **);
}       transform;

int xf_espdes __P((int, char **));
int xf_esp3des __P((int, char **));
int xf_esp3desmd5 __P((int, char **));
int xf_espdesmd5 __P((int, char **));
int xf_ahmd5 __P((int, char **));
int xf_ahsha1 __P((int, char **));
int xf_ahhmacmd5 __P((int, char **));
int xf_ahhmacsha1 __P((int, char **));
int xf_ip4 __P((int, char **));
int xf_grp __P((int, char **));
int xf_delspi __P((int, char **));
int xf_pfr __P((int, char **));

transform xf[] = {
	{"des", xf_espdes},
	{"3des", xf_esp3des},
	{"3desmd5", xf_esp3desmd5},
	{"desmd5", xf_espdesmd5},
	{"md5", xf_ahmd5},
	{"sha1", xf_ahsha1},
	{"hmacmd5", xf_ahhmacmd5},
	{"hmacsha1", xf_ahhmacsha1},
	{"ip4", xf_ip4},
	{"grp", xf_grp},
	{"delspi", xf_delspi},
	{"pfr", xf_pfr}
};

char    buf[1024];

int
x2i(char *s)
{
	char    ss[3];
	ss[0] = s[0];
	ss[1] = s[1];
	ss[2] = 0;

	return strtoul(ss, NULL, 16);
}

void
usage()
{
	fprintf( stderr, "usage: ipsecadm <operation> <args...>\n\n" );
}

int
main(argc, argv)
	int     argc;
	char  **argv;
{
	int     i;
	if (argc < 2) {
		usage();
		exit(1);
	}
	/* Find the proper transform */

	for (i = sizeof(xf) / sizeof(transform) - 1; i >= 0; i--)
		if (!strcmp(xf[i].name, argv[1])) {
			(*(xf[i].func)) (argc - 1, argv + 1);
			return 1;

		}
	usage();
        for (i = sizeof(xf) / sizeof(transform) - 1; i >= 0; i--)
        	(*(xf[i].func)) (1, &(xf[i].name));
	return 0;	
}
