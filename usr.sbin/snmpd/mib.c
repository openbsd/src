/*	$OpenBSD: mib.c,v 1.46 2011/09/16 15:44:31 yuo Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@vantronix.net>
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

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>
#include <sys/sched.h>
#include <sys/mount.h>

#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "snmpd.h"
#include "mib.h"

extern struct snmpd	*env;

/*
 * Defined in SNMPv2-MIB.txt (RFC 3418)
 */

int	 mib_getsys(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_getsnmp(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_sysor(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_setsnmp(struct oid *, struct ber_oid *, struct ber_element **);

static struct oid mib_tree[] = MIB_TREE;
static struct ber_oid zerodotzero = { { 0, 0 }, 2 };

#define sizeofa(_a) (sizeof(_a) / sizeof((_a)[0]))

/* base MIB tree */
static struct oid base_mib[] = {
	{ MIB(mib_2),			OID_MIB },
	{ MIB(sysDescr),		OID_RD, mib_getsys },
	{ MIB(sysOID),			OID_RD, mib_getsys },
	{ MIB(sysUpTime),		OID_RD, mib_getsys },
	{ MIB(sysContact),		OID_RW, mib_getsys, mps_setstr },
	{ MIB(sysName),			OID_RW, mib_getsys, mps_setstr },
	{ MIB(sysLocation),		OID_RW, mib_getsys, mps_setstr },
	{ MIB(sysServices),		OID_RS, mib_getsys },
	{ MIB(sysORLastChange),		OID_RD, mps_getts },
	{ MIB(sysORIndex),		OID_TRD, mib_sysor },
	{ MIB(sysORID),			OID_TRD, mib_sysor },
	{ MIB(sysORDescr),		OID_TRD, mib_sysor },
	{ MIB(sysORUpTime),		OID_TRD, mib_sysor },
	{ MIB(snmp),			OID_MIB },
	{ MIB(snmpInPkts),		OID_RD, mib_getsnmp },
	{ MIB(snmpOutPkts),		OID_RD, mib_getsnmp },
	{ MIB(snmpInBadVersions),	OID_RD, mib_getsnmp },
	{ MIB(snmpInBadCommunityNames),	OID_RD, mib_getsnmp },
	{ MIB(snmpInBadCommunityUses),	OID_RD, mib_getsnmp },
	{ MIB(snmpInASNParseErrs),	OID_RD, mib_getsnmp },
	{ MIB(snmpInTooBigs),		OID_RD,	mib_getsnmp },
	{ MIB(snmpInNoSuchNames),	OID_RD, mib_getsnmp },
	{ MIB(snmpInBadValues),		OID_RD, mib_getsnmp },
	{ MIB(snmpInReadOnlys),		OID_RD, mib_getsnmp },
	{ MIB(snmpInGenErrs),		OID_RD, mib_getsnmp },
	{ MIB(snmpInTotalReqVars),	OID_RD, mib_getsnmp },
	{ MIB(snmpInTotalSetVars),	OID_RD, mib_getsnmp },
	{ MIB(snmpInGetRequests),	OID_RD, mib_getsnmp },
	{ MIB(snmpInGetNexts),		OID_RD, mib_getsnmp },
	{ MIB(snmpInSetRequests),	OID_RD, mib_getsnmp },
	{ MIB(snmpInGetResponses),	OID_RD, mib_getsnmp },
	{ MIB(snmpInTraps),		OID_RD, mib_getsnmp },
	{ MIB(snmpOutTooBigs),		OID_RD, mib_getsnmp },
	{ MIB(snmpOutNoSuchNames),	OID_RD, mib_getsnmp },
	{ MIB(snmpOutBadValues),	OID_RD, mib_getsnmp },
	{ MIB(snmpOutGenErrs),		OID_RD, mib_getsnmp },
	{ MIB(snmpOutGetRequests),	OID_RD, mib_getsnmp },
	{ MIB(snmpOutGetNexts),		OID_RD, mib_getsnmp },
	{ MIB(snmpOutSetRequests),	OID_RD, mib_getsnmp },
	{ MIB(snmpOutGetResponses),	OID_RD, mib_getsnmp },
	{ MIB(snmpOutTraps),		OID_RD, mib_getsnmp },
	{ MIB(snmpEnableAuthenTraps),	OID_RW, mib_getsnmp, mib_setsnmp },
	{ MIB(snmpSilentDrops),		OID_RD, mib_getsnmp },
	{ MIB(snmpProxyDrops),		OID_RD, mib_getsnmp },
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
		*elm = ber_add_string(*elm, s);
		break;
	case 2:
		if (so == NULL)
			so = &sysoid;
		smi_oidlen(so);
		*elm = ber_add_oid(*elm, so);
		break;
	case 3:
		ticks = smi_getticks();
		*elm = ber_add_integer(*elm, ticks);
		ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
		break;
	case 4:
		if (s == NULL) {
			if (asprintf(&s, "root@%s", u.nodename) == -1)
				return (-1);
			oid->o_data = s;
			oid->o_val = strlen(s);
		}
		*elm = ber_add_string(*elm, s);
		break;
	case 5:
		if (s == NULL) {
			if ((s = strdup(u.nodename)) == NULL)
				return (-1);
			oid->o_data = s;
			oid->o_val = strlen(s);
		}
		*elm = ber_add_string(*elm, s);
		break;
	case 6:
		if (s == NULL)
			s = "";
		*elm = ber_add_string(*elm, s);
		break;
	case 7:
		*elm = ber_add_integer(*elm, oid->o_val);
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
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_sysOR]) {
	case 1:
		ber = ber_add_integer(ber, idx);
		break;
	case 2:
		ber = ber_add_oid(ber, &miboid->o_id);
		break;
	case 3:
		/*
		 * This should be a description of the MIB.
		 * But we use the symbolic OID string for now, it may
		 * help to display names of internal OIDs.
		 */
		smi_oidstring(&miboid->o_id, buf, sizeof(buf));
		ber = ber_add_string(ber, buf);
		break;
	case 4:
		/*
		 * We do not support dynamic loading of MIB at runtime,
		 * the sysORUpTime value of 0 will indicate "loaded at
		 * startup".
		 */
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber,
		    BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
mib_getsnmp(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct snmp_stats	*stats = &env->sc_stats;
	long long		 i;
	struct statsmap {
		u_int8_t	 m_id;
		u_int32_t	*m_ptr;
	}			 mapping[] = {
		{ 1, &stats->snmp_inpkts },
		{ 2, &stats->snmp_outpkts },
		{ 3, &stats->snmp_inbadversions },
		{ 4, &stats->snmp_inbadcommunitynames },
		{ 5, &stats->snmp_inbadcommunityuses },
		{ 6, &stats->snmp_inasnparseerrs },
		{ 8, &stats->snmp_intoobigs },
		{ 9, &stats->snmp_innosuchnames },
		{ 10, &stats->snmp_inbadvalues },
		{ 11, &stats->snmp_inreadonlys },
		{ 12, &stats->snmp_ingenerrs },
		{ 13, &stats->snmp_intotalreqvars },
		{ 14, &stats->snmp_intotalsetvars },
		{ 15, &stats->snmp_ingetrequests },
		{ 16, &stats->snmp_ingetnexts },
		{ 17, &stats->snmp_insetrequests },
		{ 18, &stats->snmp_ingetresponses },
		{ 19, &stats->snmp_intraps },
		{ 20, &stats->snmp_outtoobigs },
		{ 21, &stats->snmp_outnosuchnames },
		{ 22, &stats->snmp_outbadvalues },
		{ 24, &stats->snmp_outgenerrs },
		{ 25, &stats->snmp_outgetrequests },
		{ 26, &stats->snmp_outgetnexts },
		{ 27, &stats->snmp_outsetrequests },
		{ 28, &stats->snmp_outgetresponses },
		{ 29, &stats->snmp_outtraps },
		{ 31, &stats->snmp_silentdrops },
		{ 32, &stats->snmp_proxydrops }
	};

	switch (oid->o_oid[OIDIDX_snmp]) {
	case 30:
		i = stats->snmp_enableauthentraps == 1 ? 1 : 2;
		*elm = ber_add_integer(*elm, i);
		break;
	default:
		for (i = 0;
		    (u_int)i < (sizeof(mapping) / sizeof(mapping[0])); i++) {
			if (oid->o_oid[OIDIDX_snmp] == mapping[i].m_id) {
				*elm = ber_add_integer(*elm, *mapping[i].m_ptr);
				ber_set_header(*elm,
				    BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
				return (0);
			}
		}
		return (-1);
	}

	return (0);
}

int
mib_setsnmp(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct snmp_stats	*stats = &env->sc_stats;
	long long		 i;

	if (ber_get_integer(*elm, &i) == -1)
		return (-1);

	stats->snmp_enableauthentraps = i == 1 ? 1 : 0;

	return (0);
}

/*
 * Defined in HOST-RESOURCES-MIB.txt (RFC 2790)
 */

int	 mib_hrmemory(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_hrstorage(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_hrdevice(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_hrprocessor(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_hrswrun(struct oid *, struct ber_oid *, struct ber_element **);

int	 kinfo_proc_comp(const void *, const void *);
int	 kinfo_proc(u_int32_t, struct kinfo_proc **);
int	 kinfo_args(struct kinfo_proc *, char **);

static struct oid hr_mib[] = {
	{ MIB(host),				OID_MIB },
	{ MIB(hrMemorySize),			OID_RD,	mib_hrmemory },
	{ MIB(hrStorageIndex),			OID_TRD, mib_hrstorage },
	{ MIB(hrStorageType),			OID_TRD, mib_hrstorage },
	{ MIB(hrStorageDescr),			OID_TRD, mib_hrstorage },
	{ MIB(hrStorageAllocationUnits),	OID_TRD, mib_hrstorage },
	{ MIB(hrStorageSize),			OID_TRD, mib_hrstorage },
	{ MIB(hrStorageUsed),			OID_TRD, mib_hrstorage },
	{ MIB(hrStorageAllocationFailures),	OID_TRD, mib_hrstorage },
	{ MIB(hrDeviceIndex),			OID_TRD, mib_hrdevice },
	{ MIB(hrDeviceType),			OID_TRD, mib_hrdevice },
	{ MIB(hrDeviceDescr),			OID_TRD, mib_hrdevice },
	{ MIB(hrDeviceID),			OID_TRD, mib_hrdevice },
	{ MIB(hrDeviceStatus),			OID_TRD, mib_hrdevice },
	{ MIB(hrDeviceErrors),			OID_TRD, mib_hrdevice },
	{ MIB(hrProcessorFrwID),		OID_TRD, mib_hrprocessor },
	{ MIB(hrProcessorLoad),			OID_TRD, mib_hrprocessor },
	{ MIB(hrSWRunIndex),			OID_TRD, mib_hrswrun },
	{ MIB(hrSWRunName),			OID_TRD, mib_hrswrun },
	{ MIB(hrSWRunID),			OID_TRD, mib_hrswrun },
	{ MIB(hrSWRunPath),			OID_TRD, mib_hrswrun },
	{ MIB(hrSWRunParameters),		OID_TRD, mib_hrswrun },
	{ MIB(hrSWRunType),			OID_TRD, mib_hrswrun },
	{ MIB(hrSWRunStatus),			OID_TRD, mib_hrswrun },
	{ MIBEND }
};

int
mib_hrmemory(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	int			 mib[] = { CTL_HW, HW_PHYSMEM64 };
	u_int64_t		 physmem;
	size_t			 len = sizeof(physmem);

	if (sysctl(mib, sizeofa(mib), &physmem, &len, NULL, 0) == -1)
		return (-1);

	ber = ber_add_integer(ber, physmem / 1024);

	return (0);
}

int
mib_hrstorage(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx;
	struct statfs		*mntbuf, *mnt;
	int			 mntsize, maxsize;
	u_int32_t		 units, size, used, fail = 0;
	const char		*descr = NULL;
	int			 mib[] = { CTL_HW, 0 };
	u_int64_t		 physmem, realmem;
	struct uvmexp		 uvm;
	struct vmtotal		 vm;
	size_t			 len;
	static struct ber_oid	*sop, so[] = {
		{ { MIB_hrStorageOther } },
		{ { MIB_hrStorageRam } },
		{ { MIB_hrStorageVirtualMemory } },
		{ { MIB_hrStorageFixedDisk } }
	};

	/* Physical memory, real memory, swap */
	mib[1] = HW_PHYSMEM64;
	len = sizeof(physmem);
	if (sysctl(mib, sizeofa(mib), &physmem, &len, NULL, 0) == -1)
		return (-1);
	mib[1] = HW_USERMEM64;
	len = sizeof(realmem);
	if (sysctl(mib, sizeofa(mib), &realmem, &len, NULL, 0) == -1)
		return (-1);
	mib[0] = CTL_VM;
	mib[1] = VM_UVMEXP;
	len = sizeof(uvm);
	if (sysctl(mib, sizeofa(mib), &uvm, &len, NULL, 0) == -1)
		return (-1);
	mib[1] = VM_METER;
	len = sizeof(vm);
	if (sysctl(mib, sizeofa(mib), &vm, &len, NULL, 0) == -1)
		return (-1);
	maxsize = 10;

	/* Disks */
	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	if (mntsize)
		maxsize = 30 + mntsize;

	/*
	 * Get and verify the current row index.
	 *
	 * We use a special mapping here that is inspired by other SNMP
	 * agents: index 1 + 2 for RAM, index 10 for swap, index 31 and
	 * higher for disk storage.
	 */
	idx = o->bo_id[OIDIDX_hrStorageEntry];
	if (idx > (u_int)maxsize)
		return (1);
	else if (idx > 2 && idx < 10)
		idx = 10;
	else if (idx > 10 && idx < 31)
		idx = 31;

	sop = &so[0];
	switch (idx) {
	case 1:
		descr = "Physical memory";
		units = uvm.pagesize;
		size = physmem / uvm.pagesize;
		used = size - vm.t_free;
		sop = &so[1];
		break;
	case 2:
		descr = "Real memory";
		units = uvm.pagesize;
		size = realmem / uvm.pagesize;
		used = size - uvm.free;
		sop = &so[1];
		break;
	case 10:
		descr = "Swap space";
		units = uvm.pagesize;
		size = uvm.swpages;
		used = uvm.swpginuse;
		sop = &so[2];
		break;
	default:
		mnt = &mntbuf[idx - 31];
		descr = mnt->f_mntonname;
		units = mnt->f_bsize;
		size = mnt->f_blocks;
		used = mnt->f_blocks - mnt->f_bfree;
		sop = &so[3];
		break;
	}

	/* Tables need to prepend the OID on their own */
	o->bo_id[OIDIDX_hrStorageEntry] = idx;
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_hrStorage]) {
	case 1: /* hrStorageIndex */
		ber = ber_add_integer(ber, idx);
		break;
	case 2: /* hrStorageType */
		smi_oidlen(sop);
		ber = ber_add_oid(ber, sop);
		break;
	case 3: /* hrStorageDescr */
		ber = ber_add_string(ber, descr);
		break;
	case 4: /* hrStorageAllocationUnits */
		ber = ber_add_integer(ber, units);
		break;
	case 5: /* hrStorageSize */
		ber = ber_add_integer(ber, size);
		break;
	case 6: /* hrStorageUsed */
		ber = ber_add_integer(ber, used);
		break;
	case 7: /* hrStorageAllocationFailures */
		ber = ber_add_integer(ber, fail);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
mib_hrdevice(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx, fail = 0;
	int			 status;
	int			 mib[] = { CTL_HW, HW_MODEL };
	size_t			 len;
	char			 descr[BUFSIZ];
	static struct ber_oid	*sop, so[] = {
		{ { MIB_hrDeviceProcessor } },
	};

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_hrDeviceEntry];
	if (idx > (u_int)env->sc_ncpu)
		return (1);

	/* Tables need to prepend the OID on their own */
	o->bo_id[OIDIDX_hrDeviceEntry] = idx;
	ber = ber_add_oid(ber, o);

	len = sizeof(descr);
	if (sysctl(mib, sizeofa(mib), &descr, &len, NULL, 0) == -1)
		return (-1);
	/* unknown(1), running(2), warning(3), testing(4), down(5) */
	status = 2;
	sop = &so[0];

	switch (o->bo_id[OIDIDX_hrDevice]) {
	case 1: /* hrDeviceIndex */
		ber = ber_add_integer(ber, idx);
		break;
	case 2: /* hrDeviceType */
		smi_oidlen(sop);
		ber = ber_add_oid(ber, sop);
		break;
	case 3: /* hrDeviceDescr */
		ber = ber_add_string(ber, descr);
		break;
	case 4: /* hrDeviceID */
		ber = ber_add_oid(ber, &zerodotzero);
		break;
	case 5: /* hrDeviceStatus */
		ber = ber_add_integer(ber, status);
		break;
	case 6: /* hrDeviceErrors */
		ber = ber_add_integer(ber, fail);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
mib_hrprocessor(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx;
	int64_t			*cptime2, val;

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_hrDeviceEntry];
	if (idx > (u_int)env->sc_ncpu)
		return (1);
	else if (idx < 1)
		idx = 1;

	/* Tables need to prepend the OID on their own */
	o->bo_id[OIDIDX_hrDeviceEntry] = idx;
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_hrDevice]) {
	case 1: /* hrProcessorFrwID */
		ber = ber_add_oid(ber, &zerodotzero);
		break;
	case 2: /* hrProcessorLoad */
		/*
		 * The percentage of time that the system was not
		 * idle during the last minute.
		 */
		if (env->sc_cpustates == NULL)
			return (-1);
		cptime2 = env->sc_cpustates + (CPUSTATES * (idx - 1));
		val = 100 -
		    (cptime2[CP_IDLE] > 1000 ? 1000 : (cptime2[CP_IDLE] / 10));
		ber = ber_add_integer(ber, val);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
mib_hrswrun(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	struct kinfo_proc	*kinfo;
	char			*s;

	/* Get and verify the current row index */
	if (kinfo_proc(o->bo_id[OIDIDX_hrSWRunEntry], &kinfo) == -1)
		return (-1);

	if (kinfo == NULL)
		return (1);

	/* Tables need to prepend the OID on their own */
	o->bo_id[OIDIDX_hrSWRunEntry] = kinfo->p_pid;
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_hrSWRun]) {
	case 1: /* hrSWRunIndex */
		ber = ber_add_integer(ber, kinfo->p_pid);
		break;
	case 2: /* hrSWRunName */
	case 4: /* hrSWRunPath */
		ber = ber_add_string(ber, kinfo->p_comm);
		break;
	case 3: /* hrSWRunID */
		ber = ber_add_oid(ber, &zerodotzero);
		break;
	case 5: /* hrSWRunParameters */
		if (kinfo_args(kinfo, &s) == -1)
			return (-1);

		ber = ber_add_string(ber, s);
		break;
	case 6: /* hrSWRunType */
		if (kinfo->p_flag & P_SYSTEM) {
			/* operatingSystem(2) */
			ber = ber_add_integer(ber, 2);
		} else {
			/* application(4) */
			ber = ber_add_integer(ber, 4);
		}
		break;
	case 7: /* hrSWRunStatus */
		switch (kinfo->p_stat) {
		case SONPROC:
			/* running(1) */
			ber = ber_add_integer(ber, 1);
			break;
		case SIDL:
		case SRUN:
		case SSLEEP:
			/* runnable(2) */
			ber = ber_add_integer(ber, 2);
			break;
		case SSTOP:
			/* notRunnable(3) */
			ber = ber_add_integer(ber, 3);
			break;
		case SZOMB:
		case SDEAD:
		default:
			/* invalid(4) */
			ber = ber_add_integer(ber, 4);
			break;
		}
		break;
	default:
		return (-1);
	}

	return (0);
}

int
kinfo_proc_comp(const void *a, const void *b)
{
	struct kinfo_proc * const *k1 = a;
	struct kinfo_proc * const *k2 = b;

	return (((*k1)->p_pid > (*k2)->p_pid) ? 1 : -1);
}

int
kinfo_proc(u_int32_t idx, struct kinfo_proc **kinfo)
{
	static struct kinfo_proc *kp = NULL;
	static size_t		 nkp = 0;
	int			 mib[] = { CTL_KERN, KERN_PROC,
				    KERN_PROC_ALL, 0, sizeof(*kp), 0 };
	struct kinfo_proc	**klist;
	size_t			 size, count, i;

	for (;;) {
		size = nkp * sizeof(*kp);
		mib[5] = nkp;
		if (sysctl(mib, sizeofa(mib), kp, &size, NULL, 0) == -1) {
			if (errno == ENOMEM) {
				free(kp);
				kp = NULL;
				nkp = 0;
				continue;
			}

			return (-1);
		}

		count = size / sizeof(*kp);
		if (count <= nkp)
			break;

		kp = malloc(size);
		if (kp == NULL) {
			nkp = 0;
			return (-1);
		}
		nkp = count;
	}

	klist = calloc(count, sizeof(*klist));
	if (klist == NULL)
		return (-1);

	for (i = 0; i < count; i++)
		klist[i] = &kp[i];
	qsort(klist, count, sizeof(*klist), kinfo_proc_comp);

	*kinfo = NULL;
	for (i = 0; i < count; i++) {
		if (klist[i]->p_pid >= (int32_t)idx) {
			*kinfo = klist[i];
			break;
		}
	}
	free(klist);

	return (0);
}

int
kinfo_args(struct kinfo_proc *kinfo, char **s)
{
	static char		 str[128];
	static char		*buf = NULL;
	static size_t		 buflen = 128;

	int			 mib[] = { CTL_KERN, KERN_PROC_ARGS,
				    kinfo->p_pid, KERN_PROC_ARGV };
	char			*nbuf, **argv;

	if (buf == NULL) {
		buf = malloc(buflen);
		if (buf == NULL)
			return (-1);
	}

	str[0] = '\0';
	*s = str;

	while (sysctl(mib, sizeofa(mib), buf, &buflen, NULL, 0) == -1) {
		if (errno != ENOMEM) {
			/* some errors are expected, dont get too upset */
			return (0);
		}

		nbuf = realloc(buf, buflen + 128);
		if (nbuf == NULL)
			return (-1);

		buf = nbuf;
		buflen += 128;
	}

	argv = (char **)buf;
	if (argv[0] == NULL)
		return (0);

	argv++;
	while (*argv != NULL) {
		strlcat(str, *argv, sizeof(str));
		argv++;
		if (*argv != NULL)
			strlcat(str, " ", sizeof(str));
	}

	return (0);
}

/*
 * Defined in IF-MIB.txt (RFCs 1229, 1573, 2233, 2863)
 */

int	 mib_ifnumber(struct oid *, struct ber_oid *, struct ber_element **);
struct kif
	*mib_ifget(u_int);
int	 mib_iftable(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_ifxtable(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_ifstacklast(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_ifrcvtable(struct oid *, struct ber_oid *, struct ber_element **);

static u_int8_t ether_zeroaddr[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static struct oid if_mib[] = {
	{ MIB(ifMIB),			OID_MIB },
	{ MIB(ifName),			OID_TRD, mib_ifxtable },
	{ MIB(ifInMulticastPkts),	OID_TRD, mib_ifxtable },
	{ MIB(ifInBroadcastPkts),	OID_TRD, mib_ifxtable },
	{ MIB(ifOutMulticastPkts),	OID_TRD, mib_ifxtable },
	{ MIB(ifOutBroadcastPkts),	OID_TRD, mib_ifxtable },
	{ MIB(ifHCInOctets),		OID_TRD, mib_ifxtable },
	{ MIB(ifHCInUcastPkts),		OID_TRD, mib_ifxtable },
	{ MIB(ifHCInMulticastPkts),	OID_TRD, mib_ifxtable },
	{ MIB(ifHCInBroadcastPkts),	OID_TRD, mib_ifxtable },
	{ MIB(ifHCOutOctets),		OID_TRD, mib_ifxtable },
	{ MIB(ifHCOutUcastPkts),	OID_TRD, mib_ifxtable },
	{ MIB(ifHCOutMulticastPkts),	OID_TRD, mib_ifxtable },
	{ MIB(ifHCOutBroadcastPkts),	OID_TRD, mib_ifxtable },
	{ MIB(ifLinkUpDownTrapEnable),	OID_TRD, mib_ifxtable },
	{ MIB(ifHighSpeed),		OID_TRD, mib_ifxtable },
	{ MIB(ifPromiscuousMode),	OID_TRD, mib_ifxtable },
	{ MIB(ifConnectorPresent),	OID_TRD, mib_ifxtable },
	{ MIB(ifAlias),			OID_TRD, mib_ifxtable },
	{ MIB(ifCounterDiscontinuityTime), OID_TRD, mib_ifxtable },
	{ MIB(ifRcvAddressStatus),	OID_TRD, mib_ifrcvtable },
	{ MIB(ifRcvAddressType),	OID_TRD, mib_ifrcvtable },
	{ MIB(ifStackLastChange),	OID_RD, mib_ifstacklast },
	{ MIB(ifNumber),		OID_RD, mib_ifnumber },
	{ MIB(ifIndex),			OID_TRD, mib_iftable },
	{ MIB(ifDescr),			OID_TRD, mib_iftable },
	{ MIB(ifType),			OID_TRD, mib_iftable },
	{ MIB(ifMtu),			OID_TRD, mib_iftable },
	{ MIB(ifSpeed),			OID_TRD, mib_iftable },
	{ MIB(ifPhysAddress),		OID_TRD, mib_iftable },
	{ MIB(ifAdminStatus),		OID_TRD, mib_iftable },
	{ MIB(ifOperStatus),		OID_TRD, mib_iftable },
	{ MIB(ifLastChange),		OID_TRD, mib_iftable },
	{ MIB(ifInOctets),		OID_TRD, mib_iftable },
	{ MIB(ifInUcastPkts),		OID_TRD, mib_iftable },
	{ MIB(ifInNUcastPkts),		OID_TRD, mib_iftable },
	{ MIB(ifInDiscards),		OID_TRD, mib_iftable },
	{ MIB(ifInErrors),		OID_TRD, mib_iftable },
	{ MIB(ifInUnknownProtos),	OID_TRD, mib_iftable },
	{ MIB(ifOutOctets),		OID_TRD, mib_iftable },
	{ MIB(ifOutUcastPkts),		OID_TRD, mib_iftable },
	{ MIB(ifOutNUcastPkts),		OID_TRD, mib_iftable },
	{ MIB(ifOutDiscards),		OID_TRD, mib_iftable },
	{ MIB(ifOutErrors),		OID_TRD, mib_iftable },
	{ MIB(ifOutQLen),		OID_TRD, mib_iftable },
	{ MIB(ifSpecific),		OID_TRD, mib_iftable },
	{ MIBEND }
};

int
mib_ifnumber(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	*elm = ber_add_integer(*elm, kr_ifnumber());
	return (0);
}

struct kif *
mib_ifget(u_int idx)
{
	struct kif	*kif;

	if ((kif = kr_getif(idx)) == NULL) {
		/*
		 * It may happen that a interface with a specific index
		 * does not exist or has been removed. Jump to the next
		 * available interface index.
		 */
		for (kif = kr_getif(0); kif != NULL;
		    kif = kr_getnextif(kif->if_index))
			if (kif->if_index > idx)
				break;
		if (kif == NULL)
			return (NULL);
	}
	idx = kif->if_index;

	/* Update interface information */
	kr_updateif(idx);
	if ((kif = kr_getif(idx)) == NULL) {
		log_debug("mib_ifxtable: interface %d disappeared?", idx);
		return (NULL);
	}

	return (kif);
}

int
mib_iftable(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx = 0;
	struct kif		*kif;
	long long		 i;
	size_t			 len;
	int			 ifq;
	int			 mib[] = { CTL_NET, AF_INET, IPPROTO_IP, 0, 0 };

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_ifEntry];
	if ((kif = mib_ifget(idx)) == NULL)
		return (1);

	/* Tables need to prepend the OID on their own */
	o->bo_id[OIDIDX_ifEntry] = kif->if_index;
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_if]) {
	case 1:
		ber = ber_add_integer(ber, kif->if_index);
		break;
	case 2:
		/*
		 * The ifDescr should contain a vendor, product, etc.
		 * but we just use the interface name (like ifName).
		 * The interface name includes the driver name on OpenBSD.
		 */
		ber = ber_add_string(ber, kif->if_name);
		break;
	case 3:
		if (kif->if_type >= 0xf0) {
			/*
			 * It does not make sense to announce the private
			 * interface types for CARP, ENC, PFSYNC, etc.
			 */
			ber = ber_add_integer(ber, IFT_OTHER);
		} else
			ber = ber_add_integer(ber, kif->if_type);
		break;
	case 4:
		ber = ber_add_integer(ber, kif->if_mtu);
		break;
	case 5:
		ber = ber_add_integer(ber, kif->if_baudrate);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_GAUGE32);
		break;
	case 6:
		if (bcmp(kif->if_lladdr, ether_zeroaddr,
		    sizeof(kif->if_lladdr)) == 0) {
			ber = ber_add_string(ber, "");
		} else {
			ber = ber_add_nstring(ber, kif->if_lladdr,
			    sizeof(kif->if_lladdr));
		}
		break;
	case 7:
		/* ifAdminStatus up(1), down(2), testing(3) */
		i = (kif->if_flags & IFF_UP) ? 1 : 2;
		ber = ber_add_integer(ber, i);
		break;
	case 8:
		/* ifOperStatus */
		if ((kif->if_flags & IFF_UP) == 0)
			i = 2;	/* down(2) */
		else if (kif->if_link_state == LINK_STATE_UNKNOWN)
			i = 4;	/* unknown(4) */
		else if (LINK_STATE_IS_UP(kif->if_link_state))
			i = 1;	/* up(1) */
		else
			i = 7;	/* lowerLayerDown(7) or dormant(5)? */
		ber = ber_add_integer(ber, i);
		break;
	case 9:
		ber = ber_add_integer(ber, kif->if_ticks);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
		break;
	case 10:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_ibytes);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 11:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_ipackets);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 12:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_imcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 13:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_iqdrops);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 14:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_ierrors);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 15:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_noproto);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 16:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_obytes);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 17:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_opackets);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 18:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_omcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 19:
		mib[3] = IPCTL_IFQUEUE;
		mib[4] = IFQCTL_DROPS;
		len = sizeof(ifq);
		if (sysctl(mib, sizeofa(mib), &ifq, &len, 0, 0) == -1) {
			log_info("mib_iftable: %s: invalid ifq: %s",
			    kif->if_name, strerror(errno));
			return (-1);
		}
		ber = ber_add_integer(ber, ifq);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 20:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_oerrors);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 21:
		mib[3] = IPCTL_IFQUEUE;
		mib[4] = IFQCTL_LEN;
		len = sizeof(ifq);
		if (sysctl(mib, sizeofa(mib), &ifq, &len, 0, 0) == -1) {
			log_info("mib_iftable: %s: invalid ifq: %s",
			    kif->if_name, strerror(errno));
			return (-1);
		}
		ber = ber_add_integer(ber, ifq);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_GAUGE32);
		break;
	case 22:
		ber = ber_add_oid(ber, &zerodotzero);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
mib_ifxtable(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx = 0;
	struct kif		*kif;
	int			 i = 0;

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_ifXEntry];
	if ((kif = mib_ifget(idx)) == NULL)
		return (1);

	/* Tables need to prepend the OID on their own */
	o->bo_id[OIDIDX_ifXEntry] = kif->if_index;
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_ifX]) {
	case 1:
		ber = ber_add_string(ber, kif->if_name);
		break;
	case 2:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_imcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 3:
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 4:
		ber = ber_add_integer(ber, (u_int32_t)kif->if_omcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 5:
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		break;
	case 6:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_ibytes);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 7:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_ipackets);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 8:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_imcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 9:
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 10:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_obytes);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 11:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_opackets);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 12:
		ber = ber_add_integer(ber, (u_int64_t)kif->if_omcasts);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 13:
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	case 14:
		ber = ber_add_integer(ber, 0);	/* enabled(1), disabled(2) */
		break;
	case 15:
		i = kif->if_baudrate >= 1000000 ?
		    kif->if_baudrate / 1000000 : 0;
		ber = ber_add_integer(ber, i);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_GAUGE32);
		break;
	case 16:
		/* ifPromiscuousMode: true(1), false(2) */
		i = kif->if_flags & IFF_PROMISC ? 1 : 2;
		ber = ber_add_integer(ber, i);
		break;
	case 17:
		/* ifConnectorPresent: false(2), true(1) */
		i = kif->if_type == IFT_ETHER ? 1 : 2;
		ber = ber_add_integer(ber, i);
		break;
	case 18:
		ber = ber_add_string(ber, kif->if_descr);
		break;
	case 19:
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
mib_ifstacklast(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	ber = ber_add_integer(ber, kr_iflastchange());
	ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
	return (0);
}

int
mib_ifrcvtable(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx = 0;
	struct kif		*kif;
	u_int			 i = 0;

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_ifRcvAddressEntry];
	if ((kif = mib_ifget(idx)) == NULL)
		return (1);

	/*
	 * The lladdr of the interface will be encoded in the returned OID
	 * ifRcvAddressX.ifindex.6.x.x.x.x.x.x = val
	 * Thanks to the virtual cloner interfaces, it is an easy 1:1
	 * mapping in OpenBSD; only one lladdr (MAC) address per interface.
	 */

	/* first set the base OID and caluculate the length */
	idx = 0;
	o->bo_id[OIDIDX_ifRcvAddressEntry + idx++] = kif->if_index;
	o->bo_id[OIDIDX_ifRcvAddressEntry + idx] = 0;
	smi_oidlen(o);

	/* extend the OID with the lladdr length and octets */
	o->bo_id[OIDIDX_ifRcvAddressEntry + idx++] = sizeof(kif->if_lladdr);
	o->bo_n++;
	for (i = 0; i < sizeof(kif->if_lladdr); i++, o->bo_n++)
		o->bo_id[OIDIDX_ifRcvAddressEntry + idx++] = kif->if_lladdr[i];

	/* write OID */
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_ifRcvAddress]) {
	case 2:
		/* ifRcvAddressStatus: RowStatus active(1), notInService(2) */
		i = kif->if_flags & IFF_UP ? 1 : 2;
		ber = ber_add_integer(ber, i);
		break;
	case 3:
		/* ifRcvAddressType: other(1), volatile(2), nonVolatile(3) */
		ber = ber_add_integer(ber, 1);
		break;
	default:
		return (-1);
	}

	return (0);
}

/*
 * Defined in OPENBSD-SENSORS-MIB.txt
 * (http://packetmischief.ca/openbsd/snmp/)
 */

int	 mib_sensornum(struct oid *, struct ber_oid *, struct ber_element **);
int	 mib_sensors(struct oid *, struct ber_oid *, struct ber_element **);
const char *mib_sensorunit(struct sensor *);
char	*mib_sensorvalue(struct sensor *);
int	 mib_memiftable(struct oid *, struct ber_oid *, struct ber_element **);

static struct oid openbsd_mib[] = {
	{ MIB(sensorsMIBObjects),	OID_MIB },
	{ MIB(sensorNumber),		OID_RD,	mib_sensornum },
	{ MIB(sensorIndex),		OID_TRD, mib_sensors },
	{ MIB(sensorDescr),		OID_TRD, mib_sensors },
	{ MIB(sensorType),		OID_TRD, mib_sensors },
	{ MIB(sensorDevice),		OID_TRD, mib_sensors },
	{ MIB(sensorValue),		OID_TRD, mib_sensors },
	{ MIB(sensorUnits),		OID_TRD, mib_sensors },
	{ MIB(sensorStatus),		OID_TRD, mib_sensors },
	{ MIB(memMIBObjects),		OID_MIB },
	{ MIB(memMIBVersion),		OID_RD, mps_getint, NULL, NULL,
	    OIDVER_OPENBSD_MEM },
	{ MIB(memIfName),		OID_TRD, mib_memiftable },
	{ MIB(memIfLiveLocks),		OID_TRD, mib_memiftable },
	{ MIBEND }
};

int
mib_sensornum(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct sensordev	 sensordev;
	size_t			 len = sizeof(sensordev);
	int			 mib[] = { CTL_HW, HW_SENSORS, 0 };
	int			 i, c;

	for (i = c = 0; ; i++) {
		mib[2] = i;
		if (sysctl(mib, sizeofa(mib),
		    &sensordev, &len, NULL, 0) == -1) {
			if (errno == ENXIO)
				continue;
			if (errno == ENOENT)
				break;
			return (-1);
		}
		c += sensordev.sensors_count;
	}

	*elm = ber_add_integer(*elm, c);
	return (0);
}

int
mib_sensors(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	struct sensordev	 sensordev;
	size_t			 len = sizeof(sensordev);
	struct sensor		 sensor;
	size_t			 slen = sizeof(sensor);
	int			 mib[] = { CTL_HW, HW_SENSORS, 0, 0, 0 };
	int			 i, j, k;
	u_int32_t		 idx = 0, n;
	char			*s;

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_sensorEntry];

	for (i = 0, n = 1; ; i++) {
		mib[2] = i;
		if (sysctl(mib, 3, &sensordev, &len, NULL, 0) == -1) {
			if (errno == ENXIO)
				continue;
			if (errno == ENOENT)
				break;
			return (-1);
		}
		for (j = 0; j < SENSOR_MAX_TYPES; j++) {
			mib[3] = j;
			for (k = 0; k < sensordev.maxnumt[j]; k++, n++) {
				mib[4] = k;
				if (sysctl(mib, 5,
				    &sensor, &slen, NULL, 0) == -1) {
					if (errno == ENXIO)
						continue;
					if (errno == ENOENT)
						break;
					return (-1);
				}
				if (n == idx)
					goto found;
			}
		}
	}
	return (1);

 found:
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_sensor]) {
	case 1:
		ber = ber_add_integer(ber, (int32_t)n);
		break;
	case 2:
		ber = ber_add_string(ber, sensor.desc);
		break;
	case 3:
		ber = ber_add_integer(ber, sensor.type);
		break;
	case 4:
		ber = ber_add_string(ber, sensordev.xname);
		break;
	case 5:
		if ((s = mib_sensorvalue(&sensor)) == NULL)
			return (-1);
		ber = ber_add_string(ber, s);
		free(s);
		break;
	case 6:
		ber = ber_add_string(ber, mib_sensorunit(&sensor));
		break;
	case 7:
		ber = ber_add_integer(ber, sensor.status);
		break;
	}

	return (0);
}

#define SENSOR_DRIVE_STATES	(SENSOR_DRIVE_PFAIL + 1)
static const char * const sensor_drive_s[SENSOR_DRIVE_STATES] = {
	NULL, "empty", "ready", "powerup", "online", "idle", "active",
	"rebuild", "powerdown", "fail", "pfail"
};

static const char * const sensor_unit_s[SENSOR_MAX_TYPES + 1] = {
	"degC",	"RPM", "V DC", "V AC", "Ohm", "W", "A", "Wh", "Ah",
	"", "", "%", "lx", "", "sec", ""
};

const char *
mib_sensorunit(struct sensor *s)
{
	u_int	 idx;
	idx = s->type > SENSOR_MAX_TYPES ?
	    SENSOR_MAX_TYPES : s->type;
	return (sensor_unit_s[idx]);
}

char *
mib_sensorvalue(struct sensor *s)
{
	char	*v;
	int	 ret = -1;

	switch (s->type) {
	case SENSOR_TEMP:
		ret = asprintf(&v, "%.2f",
		    (s->value - 273150000) / 1000000.0);
		break;
	case SENSOR_VOLTS_DC:
	case SENSOR_VOLTS_AC:
	case SENSOR_WATTS:
	case SENSOR_AMPS:
	case SENSOR_WATTHOUR:
	case SENSOR_AMPHOUR:
	case SENSOR_LUX:
	case SENSOR_FREQ:
		ret = asprintf(&v, "%.2f", s->value / 1000000.0);
		break;
	case SENSOR_INDICATOR:
		ret = asprintf(&v, "%s", s->value ? "on" : "off");
		break;
	case SENSOR_PERCENT:
	case SENSOR_HUMIDITY:
		ret = asprintf(&v, "%.2f%%", s->value / 1000.0);
		break;
	case SENSOR_TIMEDELTA:
		ret = asprintf(&v, "%.6f", s->value / 1000000000.0);
		break;
	case SENSOR_DRIVE:
		if (s->value > 0 && s->value < SENSOR_DRIVE_STATES) {
			ret = asprintf(&v, "%s", sensor_drive_s[s->value]);
			break;
		}
		/* FALLTHROUGH */
	case SENSOR_FANRPM:
	case SENSOR_INTEGER:
	default:
		ret = asprintf(&v, "%lld", s->value);
		break;
	}

	if (ret == -1)
		return (NULL);
	return (v);
}

int
mib_memiftable(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx = 0;
	struct kif		*kif;

	idx = o->bo_id[OIDIDX_memIfEntry];
	if ((kif = mib_ifget(idx)) == NULL)
		return (1);

	o->bo_id[OIDIDX_memIfEntry] = kif->if_index;
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_memIf]) {
	case 1:
		ber = ber_add_string(ber, kif->if_name);
		break;
	case 2:
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		break;
	default:
		return (-1);
	}

	return (0);
}

/*
 * Defined in IP-MIB.txt
 */

int mib_getipstat(struct ipstat *);
int mib_ipstat(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipforwarding(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipdefaultttl(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipinhdrerrs(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipinaddrerrs(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipforwdgrams(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipindiscards(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipreasmfails(struct oid *, struct ber_oid *, struct ber_element **);
int mib_ipfragfails(struct oid *, struct ber_oid *, struct ber_element **);
int mib_iproutingdiscards(struct oid *, struct ber_oid *,
    struct ber_element **);
int mib_ipaddr(struct oid *, struct ber_oid *, struct ber_element **);
struct ber_oid *
    mib_ipaddrtable(struct oid *, struct ber_oid *, struct ber_oid *);

static struct oid ip_mib[] = {
	{ MIB(ipMIB),			OID_MIB },
	{ MIB(ipForwarding),		OID_RD, mib_ipforwarding },
	{ MIB(ipDefaultTTL),		OID_RD, mib_ipdefaultttl },
	{ MIB(ipInReceives),		OID_RD, mib_ipstat },
	{ MIB(ipInHdrErrors),		OID_RD, mib_ipinhdrerrs },
	{ MIB(ipInAddrErrors),		OID_RD, mib_ipinaddrerrs },
	{ MIB(ipForwDatagrams),		OID_RD, mib_ipforwdgrams },
	{ MIB(ipInUnknownProtos),	OID_RD, mib_ipstat },
#ifdef notyet
	{ MIB(ipInDiscards) },
#endif
	{ MIB(ipInDelivers),		OID_RD, mib_ipstat },
	{ MIB(ipOutRequests),		OID_RD, mib_ipstat },
	{ MIB(ipOutDiscards),		OID_RD, mib_ipstat },
	{ MIB(ipOutNoRoutes),		OID_RD, mib_ipstat },
	{ MIB(ipReasmTimeout),		OID_RD, mps_getint, NULL,
	    NULL, IPFRAGTTL },
	{ MIB(ipReasmReqds),		OID_RD, mib_ipstat },
	{ MIB(ipReasmOKs),		OID_RD, mib_ipstat },
	{ MIB(ipReasmFails),		OID_RD, mib_ipreasmfails },
	{ MIB(ipFragOKs),		OID_RD, mib_ipstat },
	{ MIB(ipFragFails),		OID_RD, mib_ipfragfails },
	{ MIB(ipFragCreates),		OID_RD, mib_ipstat },
	{ MIB(ipAdEntAddr),		OID_TRD, mib_ipaddr, NULL,
	    mib_ipaddrtable },
	{ MIB(ipAdEntIfIndex),		OID_TRD, mib_ipaddr, NULL,
	    mib_ipaddrtable },
	{ MIB(ipAdEntNetMask),		OID_TRD, mib_ipaddr, NULL,
	    mib_ipaddrtable },
	{ MIB(ipAdEntBcastAddr),	OID_TRD, mib_ipaddr, NULL,
	    mib_ipaddrtable },
	{ MIB(ipAdEntReasmMaxSize),	OID_TRD, mib_ipaddr, NULL,
	    mib_ipaddrtable },
#ifdef notyet
	{ MIB(ipNetToMediaIfIndex) },
	{ MIB(ipNetToMediaPhysAddress) },
	{ MIB(ipNetToMediaNetAddress) },
	{ MIB(ipNetToMediaType) },
	{ MIB(ipRoutingDiscards) },
#endif
	{ MIBEND }
};

int
mib_ipforwarding(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int	mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_FORWARDING };
	int	v;
	size_t	len = sizeof(v);

	if (sysctl(mib, sizeofa(mib), &v, &len, NULL, 0) == -1)
		return (-1);

	*elm = ber_add_integer(*elm, v);

	return (0);
}

int
mib_ipdefaultttl(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	int	mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_DEFTTL };
	int	v;
	size_t	len = sizeof(v);

	if (sysctl(mib, sizeofa(mib), &v, &len, NULL, 0) == -1)
		return (-1);

	*elm = ber_add_integer(*elm, v);

	return (0);
}

int
mib_getipstat(struct ipstat *ipstat)
{
	int	 mib[] = { CTL_NET, AF_INET, IPPROTO_IP, IPCTL_STATS };
	size_t	 len = sizeof(*ipstat);

	return (sysctl(mib, sizeofa(mib), ipstat, &len, NULL, 0));
}

int
mib_ipstat(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ipstat		 ipstat;
	long long		 i;
	struct statsmap {
		u_int8_t	 m_id;
		u_long		*m_ptr;
	}			 mapping[] = {
		{ 3, &ipstat.ips_total },
		{ 7, &ipstat.ips_noproto },
		{ 9, &ipstat.ips_delivered },
		{ 10, &ipstat.ips_localout },
		{ 11, &ipstat.ips_odropped },
		{ 12, &ipstat.ips_noroute },
		{ 14, &ipstat.ips_fragments },
		{ 15, &ipstat.ips_reassembled },
		{ 17, &ipstat.ips_fragmented },
		{ 19, &ipstat.ips_ofragments }
	};

	if (mib_getipstat(&ipstat) == -1)
		return (-1);

	for (i = 0;
	    (u_int)i < (sizeof(mapping) / sizeof(mapping[0])); i++) {
		if (oid->o_oid[OIDIDX_ip] == mapping[i].m_id) {
			*elm = ber_add_integer(*elm, *mapping[i].m_ptr);
			ber_set_header(*elm,
			    BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
			return (0);
		}
	}

	return (-1);
}

int
mib_ipinhdrerrs(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	u_int32_t	errors;
	struct ipstat	ipstat;

	if (mib_getipstat(&ipstat) == -1)
		return (-1);

	errors = ipstat.ips_badsum + ipstat.ips_badvers +
	    ipstat.ips_tooshort + ipstat.ips_toosmall +
	    ipstat.ips_badhlen +  ipstat.ips_badlen +
	    ipstat.ips_badoptions + ipstat.ips_toolong +
	    ipstat.ips_badaddr;

	*elm = ber_add_integer(*elm, errors);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipinaddrerrs(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	u_int32_t	errors;
	struct ipstat	ipstat;

	if (mib_getipstat(&ipstat) == -1)
		return (-1);

	errors = ipstat.ips_cantforward + ipstat.ips_badaddr;

	*elm = ber_add_integer(*elm, errors);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipforwdgrams(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	u_int32_t	counter;
	struct ipstat	ipstat;

	if (mib_getipstat(&ipstat) == -1)
		return (-1);

	counter = ipstat.ips_forward + ipstat.ips_redirectsent;

	*elm = ber_add_integer(*elm, counter);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipindiscards(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	return (0);
}

int
mib_ipreasmfails(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	u_int32_t	counter;
	struct ipstat	ipstat;

	if (mib_getipstat(&ipstat) == -1)
		return (-1);

	counter = ipstat.ips_fragdropped + ipstat.ips_fragtimeout;

	*elm = ber_add_integer(*elm, counter);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_ipfragfails(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	u_int32_t	counter;
	struct ipstat	ipstat;

	if (mib_getipstat(&ipstat) == -1)
		return (-1);

	counter = ipstat.ips_badfrags + ipstat.ips_cantfrag;
	*elm = ber_add_integer(*elm, counter);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);

	return (0);
}

int
mib_iproutingdiscards(struct oid *oid, struct ber_oid *o,
    struct ber_element **elm)
{
	return (0);
}

struct ber_oid *
mib_ipaddrtable(struct oid *oid, struct ber_oid *o, struct ber_oid *no)
{
	struct sockaddr_in	 addr;
	u_int32_t		 col, id;
	struct oid		 a, b;
	struct kif_addr		*ka;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_len = sizeof(addr);

	bcopy(&oid->o_id, no, sizeof(*no));
	id = oid->o_oidlen - 1;

	if (o->bo_n >= oid->o_oidlen) {
		/*
		 * Compare the requested and the matched OID to see
		 * if we have to iterate to the next element.
		 */
		bzero(&a, sizeof(a));
		bcopy(o, &a.o_id, sizeof(struct ber_oid));
		bzero(&b, sizeof(b));
		bcopy(&oid->o_id, &b.o_id, sizeof(struct ber_oid));
		b.o_oidlen--;
		b.o_flags |= OID_TABLE;
		if (smi_oid_cmp(&a, &b) == 0) {
			col = oid->o_oid[id];
			o->bo_id[id] = col;
			bcopy(o, no, sizeof(*no));
		}
	}

	mps_decodeinaddr(no, &addr.sin_addr, OIDIDX_ipAddr + 1);
	if (addr.sin_addr.s_addr == INADDR_ANY)
		ka = kr_getaddr(NULL);
	else
		ka = kr_getnextaddr((struct sockaddr *)&addr);
	if (ka == NULL || ka->addr.sa.sa_family != AF_INET)
		addr.sin_addr.s_addr = 0;
	else
		addr.sin_addr.s_addr = ka->addr.sin.sin_addr.s_addr;
	mps_encodeinaddr(no, &addr.sin_addr, OIDIDX_ipAddr + 1);
	smi_oidlen(o);

	return (no);
}

int
mib_ipaddr(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct sockaddr_in	 addr;
	struct ber_element	*ber = *elm;
	struct kif_addr		*ka;
	u_int32_t		 val;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_len = sizeof(addr);

	mps_decodeinaddr(o, &addr.sin_addr, OIDIDX_ipAddr + 1);
	ka = kr_getaddr((struct sockaddr *)&addr);
	if (ka == NULL || ka->addr.sa.sa_family != AF_INET)
		return (1);

	/* write OID */
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_ipAddr]) {
	case 1:
		val = addr.sin_addr.s_addr;
		ber = ber_add_nstring(ber, (char *)&val, sizeof(u_int32_t));
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_IPADDR);
		break;
	case 2:
		ber = ber_add_integer(ber, ka->if_index);
		break;
	case 3:
		val = ka->mask.sin.sin_addr.s_addr;
		ber = ber_add_nstring(ber, (char *)&val, sizeof(u_int32_t));
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_IPADDR);
		break;
	case 4:
		ber = ber_add_integer(ber, ka->dstbrd.sa.sa_len ? 1 : 0);
		break;
	case 5:
		ber = ber_add_integer(ber, IP_MAXPACKET);
		break;
	default:
		return (-1);
	}

	return (0);
}

/*
 * Defined in IP-FORWARD-MIB.txt (rfc4292)
 */

int mib_ipfnroutes(struct oid *, struct ber_oid *, struct ber_element **);
struct ber_oid *
mib_ipfroutetable(struct oid *oid, struct ber_oid *o, struct ber_oid *no);
int mib_ipfroute(struct oid *, struct ber_oid *, struct ber_element **);

static struct oid ipf_mib[] = {
	{ MIB(ipfMIB),			OID_MIB },
	{ MIB(ipfInetCidrRouteNumber),	OID_RD, mib_ipfnroutes },

	{ MIB(ipfRouteEntIfIndex),	OID_TRD, mib_ipfroute, NULL,
	    mib_ipfroutetable },
	{ MIB(ipfRouteEntType),		OID_TRD, mib_ipfroute, NULL,
	    mib_ipfroutetable },
	{ MIB(ipfRouteEntProto),	OID_TRD, mib_ipfroute, NULL,
	    mib_ipfroutetable },
	{ MIB(ipfRouteEntAge),		OID_TRD, mib_ipfroute, NULL,
	    mib_ipfroutetable },
	{ MIB(ipfRouteEntNextHopAS),	OID_TRD, mib_ipfroute, NULL,
	    mib_ipfroutetable },
	{ MIB(ipfRouteEntRouteMetric1),	OID_TRD, mib_ipfroute, NULL,
	    mib_ipfroutetable },
	{ MIB(ipfRouteEntRouteMetric2),	OID_TRD, mib_ipfroute, NULL,
	    mib_ipfroutetable },
	{ MIB(ipfRouteEntRouteMetric3),	OID_TRD, mib_ipfroute, NULL,
	    mib_ipfroutetable },
	{ MIB(ipfRouteEntRouteMetric4),	OID_TRD, mib_ipfroute, NULL,
	    mib_ipfroutetable },
	{ MIB(ipfRouteEntRouteMetric5),	OID_TRD, mib_ipfroute, NULL,
	    mib_ipfroutetable },
	{ MIB(ipfRouteEntStatus),	OID_TRD, mib_ipfroute, NULL,
	    mib_ipfroutetable },
	{ MIBEND }
};

int
mib_ipfnroutes(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	*elm = ber_add_integer(*elm, kr_routenumber());
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_GAUGE32);
	
	return (0);
}

struct ber_oid *
mib_ipfroutetable(struct oid *oid, struct ber_oid *o, struct ber_oid *no)
{
	u_int32_t		 col, id;
	struct oid		 a, b;
	struct sockaddr_in	 addr;
	struct kroute		*kr;
	int			 af, atype, idx;
	u_int8_t		 prefixlen;
	u_int8_t		 prio;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_len = sizeof(addr);

	bcopy(&oid->o_id, no, sizeof(*no));
	id = oid->o_oidlen - 1;

	if (o->bo_n >= oid->o_oidlen) {
		/*
		 * Compare the requested and the matched OID to see
		 * if we have to iterate to the next element.
		 */
		bzero(&a, sizeof(a));
		bcopy(o, &a.o_id, sizeof(struct ber_oid));
		bzero(&b, sizeof(b));
		bcopy(&oid->o_id, &b.o_id, sizeof(struct ber_oid));
		b.o_oidlen--;
		b.o_flags |= OID_TABLE;
		if (smi_oid_cmp(&a, &b) == 0) {
			col = oid->o_oid[id];
			o->bo_id[id] = col;
			bcopy(o, no, sizeof(*no));
		}
	}

	af = no->bo_id[OIDIDX_ipfInetCidrRoute + 1];
	mps_decodeinaddr(no, &addr.sin_addr, OIDIDX_ipfInetCidrRoute + 3);
	prefixlen = o->bo_id[OIDIDX_ipfInetCidrRoute + 7];
	prio = o->bo_id[OIDIDX_ipfInetCidrRoute + 10];

	if (af == 0)
		kr = kroute_first();
	else
		kr = kroute_getaddr(addr.sin_addr.s_addr, prefixlen, prio, 1);

	if (kr == NULL) {
		addr.sin_addr.s_addr = 0;
		prefixlen = 0;
		prio = 0;
		addr.sin_family = 0;
	} else {
		addr.sin_addr.s_addr = kr->prefix.s_addr;
		prefixlen = kr->prefixlen;
		prio = kr->priority;
	}

	switch(addr.sin_family) {
	case AF_INET:
		atype = 1;
		break;
	case AF_INET6:
		atype = 2;
		break;
	default:
		atype = 0;
		break;
	}
	idx = OIDIDX_ipfInetCidrRoute + 1;
	no->bo_id[idx++] = atype;
	no->bo_id[idx++] = 0x04;
	no->bo_n++;

	mps_encodeinaddr(no, &addr.sin_addr, idx);
	no->bo_id[no->bo_n++] = prefixlen;
	no->bo_id[no->bo_n++] = 0x02;
	no->bo_n += 2; /* policy */
	no->bo_id[OIDIDX_ipfInetCidrRoute + 10]  = prio;

	if (kr != NULL) {
		no->bo_id[no->bo_n++] = atype;
		no->bo_id[no->bo_n++] = 0x04;
		mps_encodeinaddr(no, &kr->nexthop, no->bo_n);
	} else
		no->bo_n += 2;

	smi_oidlen(o);

	return (no);
}

int
mib_ipfroute(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	struct kroute		*kr;
	struct sockaddr_in	 addr, nhaddr;
	int			 idx = o->bo_id[OIDIDX_ipfInetCidrRoute];
	int			 af;
	u_int8_t		 prefixlen, prio, type, proto;


	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_len = sizeof(addr);

	af = o->bo_id[OIDIDX_ipfInetCidrRoute + 1];
	mps_decodeinaddr(o, &addr.sin_addr, OIDIDX_ipfInetCidrRoute + 3);
	mps_decodeinaddr(o, &nhaddr.sin_addr, OIDIDX_ipfInetCidrRoute + 23);
	prefixlen = o->bo_id[OIDIDX_ipfInetCidrRoute + 7];
	prio = o->bo_id[OIDIDX_ipfInetCidrRoute + 10];
	kr = kroute_getaddr(addr.sin_addr.s_addr, prefixlen, prio, 0);
	if (kr == NULL || af == 0) {
		return (1);
	}

	/* write OID */
	ber = ber_add_oid(ber, o);

	switch (idx) {
	case 7: /* IfIndex */
		ber = ber_add_integer(ber, kr->if_index);
		break;
	case 8: /* Type */
		if (kr->flags & F_REJECT)
			type = 2;
		else if (kr->flags & F_BLACKHOLE)
			type = 5;
		else if (kr->flags & F_CONNECTED)
			type = 3;
		else
			type = 4;
		ber = ber_add_integer(ber, type);
		break;
	case 9: /* Proto */
		switch (kr->priority) {
		case RTP_CONNECTED:
			proto = 2;
			break;
		case RTP_STATIC:
			proto = 3;
			break;
		case RTP_OSPF:
			proto = 13;
			break;
		case RTP_ISIS:
			proto = 9;
			break;
		case RTP_RIP:
			proto = 8;
			break;
		case RTP_BGP:
			proto = 14;
			break;
		default:
			if (kr->flags & F_DYNAMIC)
				proto = 4;
			else
				proto = 1; /* not specified */
			break;
		}
		ber = ber_add_integer(ber, proto);
		break;
	case 10: /* Age */
		ber = ber_add_integer(ber, 0);
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_GAUGE32);
		break;
	case 11: /* NextHopAS */
		ber = ber_add_integer(ber, 0);	/* unknown */
		ber_set_header(ber, BER_CLASS_APPLICATION, SNMP_T_GAUGE32);
		break;
	case 12: /* Metric1 */
		ber = ber_add_integer(ber, -1);	/* XXX */
		break;
	case 13: /* Metric2 */
		ber = ber_add_integer(ber, -1);	/* XXX */
		break;
	case 14: /* Metric3 */
		ber = ber_add_integer(ber, -1);	/* XXX */
		break;
	case 15: /* Metric4 */
		ber = ber_add_integer(ber, -1);	/* XXX */
		break;
	case 16: /* Metric5 */
		ber = ber_add_integer(ber, -1);	/* XXX */
		break;
	case 17: /* Status */
		ber = ber_add_integer(ber, 1);	/* XXX */
		break;
	default:
		return (-1);
	}

	return (0);
}

/*
 * Defined in BRIDGE-MIB.txt (rfc1493)
 *
 * This MIB is required by some NMS to accept the device because
 * the RFC says that mostly any network device has to provide this MIB... :(
 */

int	 mib_dot1dtable(struct oid *, struct ber_oid *, struct ber_element **);

static struct oid bridge_mib[] = {
	{ MIB(dot1dBridge),		OID_MIB },
	{ MIB(dot1dBaseBridgeAddress) },
	{ MIB(dot1dBaseNumPorts),	OID_RD, mib_ifnumber },
	{ MIB(dot1dBaseType),		OID_RD, mps_getint, NULL,
	    NULL, 4 /* srt (sourceroute + transparent) */ },
	{ MIB(dot1dBasePort),		OID_TRD, mib_dot1dtable },
	{ MIB(dot1dBasePortIfIndex),	OID_TRD, mib_dot1dtable },
	{ MIB(dot1dBasePortCircuit),	OID_TRD, mib_dot1dtable},
	{ MIB(dot1dBasePortDelayExceededDiscards), OID_TRD, mib_dot1dtable },
	{ MIB(dot1dBasePortMtuExceededDiscards), OID_TRD, mib_dot1dtable },
	{ MIBEND }
};

int
mib_dot1dtable(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	u_int32_t		 idx = 0;
	struct kif		*kif;

	/* Get and verify the current row index */
	idx = o->bo_id[OIDIDX_dot1dEntry];
	if ((kif = mib_ifget(idx)) == NULL)
		return (1);

	/* Tables need to prepend the OID on their own */
	o->bo_id[OIDIDX_dot1dEntry] = kif->if_index;
	ber = ber_add_oid(ber, o);

	switch (o->bo_id[OIDIDX_dot1d]) {
	case 1:
	case 2:
		ber = ber_add_integer(ber, kif->if_index);
		break;
	case 3:
		ber = ber_add_oid(ber, &zerodotzero);
		break;
	case 4:
	case 5:
		ber = ber_add_integer(ber, 0);
		break;
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

	/* HOST-RESOURCES-MIB */
	smi_mibtree(hr_mib);

	/* IF-MIB */
	smi_mibtree(if_mib);

	/* IP-MIB */
	smi_mibtree(ip_mib);

	/* IP-FORWARD-MIB */
	smi_mibtree(ipf_mib);

	/* BRIDGE-MIB */
	smi_mibtree(bridge_mib);

	/* OPENBSD-MIB */
	smi_mibtree(openbsd_mib);
}
