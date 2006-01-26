/*	$OpenBSD: carp.c,v 1.2 2006/01/26 09:53:46 moritz Exp $	*/

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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ip_carp.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sasyncd.h"

/* For some reason, ip_carp.h does not define this.  */
#define CARP_INIT	0
#define CARP_BACKUP	1
#define CARP_MASTER	2

/* Returns 1 for the CARP MASTER, 0 for BACKUP/INIT, -1 on error.  */
static enum RUNSTATE
carp_get_state(char *ifname)
{
	struct ifreq	ifr;
	struct carpreq	carp;
	int		s, saved_errno;
	char		*state;

	if (!ifname || !*ifname) {
		errno = ENOENT;
		return FAIL;
	}

	memset(&ifr, 0, sizeof ifr);
	strlcpy(ifr.ifr_name, ifname, sizeof ifr.ifr_name);

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return FAIL;

	ifr.ifr_data = (caddr_t)&carp;
	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1) {
		saved_errno = errno;
		close(s);
		errno = saved_errno;
		return FAIL;
	}
	close(s);

	switch (carp.carpr_state) {
	case CARP_INIT:
		state = "INIT";
		break;

	case CARP_BACKUP:
		state = "BACKUP";
		break;

	case CARP_MASTER:
		state = "MASTER";
		break;

	default:
		state = "<unknown>";
		break;
	}

	log_msg(4, "carp_get_state: %s vhid %d state %s(%d)", ifname,
	    carp.carpr_vhid, state, carp.carpr_state);

	if (carp.carpr_vhid > 0)
		return carp.carpr_state == CARP_MASTER ? MASTER : SLAVE;
	else
		return FAIL;
}

void
carp_check_state(void)
{
	enum RUNSTATE	current_state = carp_get_state(cfgstate.carp_ifname);
	static char	*carpstate[] = CARPSTATES;

	if (current_state < 0 || current_state > FAIL) {
		log_err("carp_state_tracker: invalid result on interface "
		    "%s, abort", cfgstate.carp_ifname);
		cfgstate.runstate = FAIL;
		return;
	}

	if (current_state != cfgstate.runstate) {
		log_msg(1, "carp_state_tracker: switching state to %s",
		    carpstate[current_state]);
		cfgstate.runstate = current_state;
		if (current_state == MASTER)
			pfkey_set_promisc();
		net_ctl_update_state();
	}
}

static void
carp_state_tracker(void *v_arg)
{
	static int	failures = 0;
	u_int32_t	next_check;

	carp_check_state();

	if (cfgstate.runstate == FAIL)
		if (++failures < 3)
			log_err("carp_state_tracker");

	if (failures > 5)
		next_check = 600;
	else
		next_check = cfgstate.carp_check_interval + failures * 10;

	if (timer_add("carp_state_tracker", next_check, carp_state_tracker,
	    NULL))
		log_msg(0, "carp_state_tracker: failed to renew event");
	return;
}

/* Initialize the CARP state tracker. */
int
carp_init(void)
{
	enum RUNSTATE initial_state;

	if (cfgstate.lockedstate != INIT) {
		cfgstate.runstate = cfgstate.lockedstate;
		log_msg(1, "carp_init: locking runstate to %s",
		    cfgstate.runstate == MASTER ? "MASTER" : "SLAVE");
		return 0;
	}

	initial_state = carp_get_state(cfgstate.carp_ifname);
	if (initial_state == FAIL) {
		fprintf(stderr, "Failed to check interface \"%s\".\n",
		    cfgstate.carp_ifname);
		fprintf(stderr, "Correct or manually select runstate.\n");
		return -1;
	}

	return timer_add("carp_state_tracker", 0, carp_state_tracker, NULL);
}
