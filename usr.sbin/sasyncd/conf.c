/*	$OpenBSD: conf.c,v 1.1 2005/03/30 18:44:49 ho Exp $	*/

/*
 * Copyright (c) 2005 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Multicom Security AB.
 */


#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sasyncd.h"
#include "net.h"

/* Global configuration context.  */
struct cfgstate	cfgstate;

static int
conf_parse_file(char *cfgfile)
{
	struct syncpeer	*peer;
	FILE	*fp = fopen(cfgfile, "r");
	int	 lineno = 0, dup;
	char	 linebuf[1024], name[1024], *p;

	if (!fp) {
		log_err("failed to open \"%s\"", cfgfile);
		return 1;
	}

	while(fgets(linebuf, sizeof linebuf, fp)) {
		lineno++;

		/* Strip comments and remove \n.  */
		for (p = linebuf; *p; p++)
			if (*p == '\n' || *p == '#') {
				*p = 0;
				break;
			}

		if (*linebuf == 0)
			continue;

		if (sscanf(linebuf, "listen on %1023s", name) == 1) {
			if (cfgstate.listen_on)
				free(cfgstate.listen_on);
			cfgstate.listen_on = strdup(name);
			if (!cfgstate.listen_on) {
				log_err("config: strdup() failed");
				return 1;
			}
			log_msg(2, "config(line %02d): listen on %s", lineno,
			    cfgstate.listen_on);
			continue;
		}

		if (sscanf(linebuf, "listen port %1023s", name) == 1) {
			cfgstate.listen_port = atoi(name);
			if (cfgstate.listen_port < 1 ||
			    cfgstate.listen_port > 65534) {
				cfgstate.listen_port = SASYNCD_DEFAULT_PORT;
				log_msg(0, "config: bad value line %d, "
				    "listen-port reset to %u", lineno,
				    SASYNCD_DEFAULT_PORT);
			} else
				log_msg(2, "config(line %02d): listen port %u",
				    cfgstate.listen_port);
			continue;
		}

		if (sscanf(linebuf, "peer %1023s", name) == 1) {
			dup = 0;
			for (peer = LIST_FIRST(&cfgstate.peerlist); peer;
			     peer = LIST_NEXT(peer, link))
				if (strcmp(name, peer->name) == 0) {
					dup++;
					break;
				}
			if (dup)
				continue;
			peer = (struct syncpeer *)calloc(1, sizeof *peer);
			if (!peer) {
				log_err("config: calloc(1, %lu) failed",
				    sizeof *peer);
				return 1;
			}
			peer->name = strdup(name);
			if (!peer->name) {
				log_err("config: strdup() failed");
				return 1;
			}
			LIST_INSERT_HEAD(&cfgstate.peerlist, peer, link);
			log_msg(2, "config(line %02d): add peer %s", lineno,
			    peer->name);
			continue;
		}

		if (sscanf(linebuf, "carp interface %1023s", name) == 1) {
			if (cfgstate.carp_ifname)
				free(cfgstate.carp_ifname);
			cfgstate.carp_ifname = strdup(name);
			if (!cfgstate.carp_ifname) {
				log_err("config: strdup failed");
				return 1;
			}
			log_msg(2, "config(line %02d): carp interface %s",
			    lineno, cfgstate.carp_ifname);
			continue;
		}

		if (sscanf(linebuf, "carp interval %1023s", name) == 1) {
			cfgstate.carp_check_interval = atoi(name);
			if (cfgstate.carp_check_interval < 1) {
				cfgstate.carp_check_interval = 
				    CARP_DEFAULT_INTERVAL;
				log_msg(0, "config: bad value line %d, "
				    "carp-interval reset to %d", lineno,
				    CARP_DEFAULT_INTERVAL);
			} else
				log_msg(2, "config(line %02d): carp interval "
				    "%d", lineno,
				    cfgstate.carp_check_interval);
			continue;
		}

		if (sscanf(linebuf, "CAcertificate file %1023s", name) == 1) {
			if (cfgstate.cafile)
				free(cfgstate.cafile);
			cfgstate.cafile = strdup(name);
			if (!cfgstate.cafile) {
				log_err("config: strdup failed");
				return 1;
			}
			log_msg(2, "config(line %02d): CAcertificate file $s",
			    lineno, cfgstate.cafile);
			continue;
		}

		if (sscanf(linebuf, "certificate file %1023s", name) == 1) {
			if (cfgstate.certfile)
				free(cfgstate.certfile);
			cfgstate.certfile = strdup(name);
			if (!cfgstate.certfile) {
				log_err("config: strdup failed");
				return 1;
			}
			log_msg(2, "config(line %02d): certificate file $s",
			    lineno, cfgstate.certfile);
			continue;
		}

		if (sscanf(linebuf, "private key file %1023s", name) == 1) {
			if (cfgstate.privkeyfile)
				free(cfgstate.privkeyfile);
			cfgstate.privkeyfile = strdup(name);
			if (!cfgstate.privkeyfile) {
				log_err("config: strdup failed");
				return 1;
			}
			log_msg(2, "config(line %02d): private key file $s",
			    lineno, cfgstate.privkeyfile);
			continue;
		}

		if (sscanf(linebuf, "run as %1023s", name) == 1) {
			if (strcasecmp(name, "SLAVE") == 0)
				cfgstate.lockedstate = SLAVE;
			else if (strcasecmp(name, "MASTER") == 0)
				cfgstate.lockedstate = MASTER;
			else {
				log_msg(0, "config(line %02d): unknown state "
				    "%s", lineno, name);
				return 1;
			}
			log_msg(2, "config(line %02d): runstate locked to %s",
			    lineno, cfgstate.lockedstate == MASTER ? "MASTER" :
			    "SLAVE");
			continue;
		}
	}

	/* Sanity checks. */
	if (LIST_EMPTY(&cfgstate.peerlist)) {
		log_msg(0, "config: no peers defined");
		return 1;
	}

	if (!cfgstate.carp_ifname && cfgstate.lockedstate == INIT) {
		log_msg(0, "config: no carp interface or runstate defined");
		return 1;
	}

	/* Success. */
	return 0;
}

int
conf_init(int argc, char **argv)
{
	char	*cfgfile = 0;
	int	 ch;

	memset(&cfgstate, 0, sizeof cfgstate);
	cfgstate.runstate = INIT;
	LIST_INIT(&cfgstate.peerlist);

	cfgstate.carp_check_interval = CARP_DEFAULT_INTERVAL;
	cfgstate.listen_port = SASYNCD_DEFAULT_PORT;

	while ((ch = getopt(argc, argv, "c:dv")) != -1) {
		switch (ch) {
		case 'c':
			if (cfgfile)
				return 2;
			cfgfile = optarg;
			break;
		case 'd':
			cfgstate.debug++;
			break;
		case 'v':
			cfgstate.verboselevel++;
			break;
		default:
			return 2;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		return 2;

	if (!cfgfile)
		cfgfile = SASYNCD_CFGFILE;

	if (conf_parse_file(cfgfile) == 0) {
		if (!cfgstate.certfile)
			cfgstate.certfile = SASYNCD_CERTFILE;
		if (!cfgstate.privkeyfile)
			cfgstate.privkeyfile = SASYNCD_PRIVKEY;
		if (!cfgstate.cafile)
			cfgstate.cafile = SASYNCD_CAFILE;
		return 0;
	} else
		return 1;
}
