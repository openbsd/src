/*	$OpenBSD: mib.c,v 1.107 2023/11/04 09:30:28 martijn Exp $	*/

/*
 * Copyright (c) 2012 Joel Knight <joel@openbsd.org>
 * Copyright (c) 2007, 2008, 2012 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/tree.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>
#include <sys/sched.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/disk.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_carp.h>
#include <netinet/ip_var.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/pfvar.h>
#include <netinet/ip_ipsp.h>
#include <net/if_pfsync.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <kvm.h>

#include "snmpd.h"
#include "mib.h"

/*
 * Defined in SNMPv2-MIB.txt (RFC 3418)
 */

int	 mib_getsys(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_sysor(struct oid *, struct ber_oid *, struct ber_element **);

static struct oid mib_tree[] = MIB_TREE;

/* base MIB tree */
static struct oid base_mib[] = {
	{ MIB(mib_2),			OID_MIB },
	{ MIB(sysDescr),		OID_RD, mib_getsys },
	{ MIB(sysOID),			OID_RD, mib_getsys },
	{ MIB(sysUpTime),		OID_RD, mib_getsys },
	{ MIB(sysContact),		OID_RD, mib_getsys },
	{ MIB(sysName),			OID_RD, mib_getsys },
	{ MIB(sysLocation),		OID_RD, mib_getsys },
	{ MIB(sysServices),		OID_RS, mib_getsys },
	{ MIB(sysORLastChange),		OID_RD, mps_getts },
	{ MIB(sysORIndex),		OID_TRD, mib_sysor },
	{ MIB(sysORID),			OID_TRD, mib_sysor },
	{ MIB(sysORDescr),		OID_TRD, mib_sysor },
	{ MIB(sysORUpTime),		OID_TRD, mib_sysor },
	{ MIBEND }
};

int
mib_getsys(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_oid		 sysoid = OID(MIB_SYSOID_DEFAULT);
	char			*s = oid->o_data;
	struct ber_oid		*so = oid->o_data;
	struct utsname		 u;
	long long		 ticks;

	if (uname(&u) == -1)
		return (-1);

	switch (oid->o_oid[OIDIDX_system]) {
	case 1:
		if (s == NULL) {
			if (asprintf(&s, "%s %s %s %s %s",
			    u.sysname, u.nodename, u.release,
			    u.version, u.machine) == -1)
				return (-1);
			oid->o_data = s;
			oid->o_val = strlen(s);
		}
		*elm = ober_add_string(*elm, s);
		break;
	case 2:
		if (so == NULL)
			so = &sysoid;
		smi_oidlen(so);
		*elm = ober_add_oid(*elm, so);
		break;
	case 3:
		ticks = smi_getticks();
		*elm = ober_add_integer(*elm, ticks);
		ober_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
		break;
	case 4:
		if (s == NULL) {
			if (asprintf(&s, "root@%s", u.nodename) == -1)
				return (-1);
			oid->o_data = s;
			oid->o_val = strlen(s);
		}
		*elm = ober_add_string(*elm, s);
		break;
	case 5:
		if (s == NULL) {
			if ((s = strdup(u.nodename)) == NULL)
				return (-1);
			oid->o_data = s;
			oid->o_val = strlen(s);
		}
		*elm = ober_add_string(*elm, s);
		break;
	case 6:
		if (s == NULL)
			s = "";
		*elm = ober_add_string(*elm, s);
		break;
	case 7:
		*elm = ober_add_integer(*elm, oid->o_val);
		break;
	default:
		return (-1);
	}
	return (0);
}

int
mib_sysor(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx = 1, nmib = 0;
	struct oid		*next, *miboid;
	char			 buf[SNMPD_MAXSTRLEN];

	/* Count MIB root OIDs in the tree */
	for (next = NULL;
	    (next = smi_foreach(next, OID_MIB)) != NULL; nmib++);

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_sysOREntry];
	if (idx > nmib)
		return (1);

	/* Find the MIB root element for this Id */
	for (next = miboid = NULL, nmib = 1;
	    (next = smi_foreach(next, OID_MIB)) != NULL; nmib++) {
		if (nmib == idx)
			miboid = next;
	}
	if (miboid == NULL)
		return (-1);

	/* Tables need to prepend the OID on their own */
	ber = ober_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_sysOR]) {
	case 1:
		ber = ober_add_integer(ber, idx);
		break;
	case 2:
		ber = ober_add_oid(ber, &miboid->o_id);
		break;
	case 3:
		/*
		 * This should be a description of the MIB.
		 * But we use the symbolic OID string for now, it may
		 * help to display names of internal OIDs.
		 */
		smi_oid2string(&miboid->o_id, buf, sizeof(buf), 0);
		ber = ober_add_string(ber, buf);
		break;
	case 4:
		/*
		 * We do not support dynamic loading of MIB at runtime,
		 * the sysORUpTime value of 0 will indicate "loaded at
		 * startup".
		 */
		ber = ober_add_integer(ber, 0);
		ober_set_header(ber,
		    BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
		break;
	default:
		return (-1);
	}

	return (0);
}

/*
 * Import all MIBs
 */

void
mib_init(void)
{
	/*
	 * MIB declarations (to register the OID names)
	 */
	smi_mibtree(mib_tree);

	/*
	 * MIB definitions (the implementation)
	 */

	/* SNMPv2-MIB */
	smi_mibtree(base_mib);
}
