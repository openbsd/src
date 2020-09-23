/*	$OpenBSD: control_agentx.c,v 1.1 2020/09/23 15:52:06 martijn Exp $ */

/*
 * Copyright (c) 2020 Martijn van Duren <martijn@openbsd.org>
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
#include <sys/sysctl.h>
#include <sys/un.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <subagentx.h>
#include <unistd.h>

#include "proc.h"
#include "vmd.h"

#define CONTROL_AGENTX_PEERID UINT32_MAX

struct control_agentx_vb {
	struct subagentx_varbind *cav_vb;
	struct subagentx_index *cav_idx;
};

static void control_agentx_nofd(struct subagentx *, void *, int);
static void control_agentx_tryconnect(int, short, void *);
static void control_agentx_read(int, short, void *);
static int control_agentx_sortvir(const void *, const void *);
static int control_agentx_adminstate(int);
static int control_agentx_operstate(int);
static void control_agentx_vmHvSoftware(struct subagentx_varbind *);
static void control_agentx_vmHvVersion(struct subagentx_varbind *);
static void control_agentx_vmHvObjectID(struct subagentx_varbind *);
static void control_agentx_get_vmInfo(struct control_agentx_vb **, size_t *,
    size_t *, struct subagentx_varbind *);
static void control_agentx_vmNumber(struct subagentx_varbind *);
static void control_agentx_emptystring_vm(struct subagentx_varbind *);
static void control_agentx_vmName(struct subagentx_varbind *);
static void control_agentx_vmAdminState(struct subagentx_varbind *);
static void control_agentx_vmOperState(struct subagentx_varbind *);
static void control_agentx_vmAutoStart(struct subagentx_varbind *);
static void control_agentx_vmPersistent(struct subagentx_varbind *);
static void control_agentx_vmCurCpuNumber(struct subagentx_varbind *);
static void control_agentx_vmMemUnit(struct subagentx_varbind *);
static void control_agentx_vmCurMem(struct subagentx_varbind *);
static void control_agentx_vmMinMem(struct subagentx_varbind *);
static void control_agentx_vmMaxMem(struct subagentx_varbind *);
static void control_agentx_vm_finalize(struct control_agentx_vb *,
    struct vmop_info_result *, uint32_t,
    void (*)(struct subagentx_varbind *, struct vmop_info_result *));
static void control_agentx_emptystring_vm_finalize(struct subagentx_varbind *,
    struct vmop_info_result *);
static void control_agentx_vmName_finalize(struct subagentx_varbind *,
    struct vmop_info_result *);
static void control_agentx_vmAdminState_finalize(struct subagentx_varbind *,
    struct vmop_info_result *);
static void control_agentx_vmOperState_finalize(struct subagentx_varbind *,
    struct vmop_info_result *);
static void control_agentx_vmAutoStart_finalize(struct subagentx_varbind *,
    struct vmop_info_result *);
static void control_agentx_vmPersistent_finalize(struct subagentx_varbind *,
    struct vmop_info_result *);
static void control_agentx_vmCurCpuNumber_finalize(struct subagentx_varbind *,
    struct vmop_info_result *);
static void control_agentx_vmMemUnit_finalize(struct subagentx_varbind *,
    struct vmop_info_result *);
static void control_agentx_vmCurMem_finalize(struct subagentx_varbind *,
    struct vmop_info_result *);
static void control_agentx_vmMinMem_finalize(struct subagentx_varbind *,
    struct vmop_info_result *);
static void control_agentx_vmMaxMem_finalize(struct subagentx_varbind *,
    struct vmop_info_result *);

static struct subagentx *sa;
static struct subagentx_session *sas;
static struct subagentx_context *sac;
static struct subagentx_region *vmMIB;
static struct subagentx_index *vmIndex;
static struct subagentx_object *vmNumber;
static struct event connev, rev;
static struct agentx *agentx;
static struct privsep *aps;

static struct control_agentx_vb *vmNumbervb = NULL;
static size_t vmNumbernvb = 0;
static size_t vmNumbervblen = 0;
static struct control_agentx_vb *vmEmptystringvb = NULL;
static size_t vmEmptystringnvb = 0;
static size_t vmEmptystringvblen = 0;
static struct control_agentx_vb *vmNamevb = NULL;
static size_t vmNamenvb = 0;
static size_t vmNamevblen = 0;
static struct control_agentx_vb *vmAdminStatevb = NULL;
static size_t vmAdminStatenvb = 0;
static size_t vmAdminStatevblen = 0;
static struct control_agentx_vb *vmOperStatevb = NULL;
static size_t vmOperStatenvb = 0;
static size_t vmOperStatevblen = 0;
static struct control_agentx_vb *vmAutoStartvb = NULL;
static size_t vmAutoStartnvb = 0;
static size_t vmAutoStartvblen = 0;
static struct control_agentx_vb *vmPersistentvb = NULL;
static size_t vmPersistentnvb = 0;
static size_t vmPersistentvblen = 0;
static struct control_agentx_vb *vmCurCpuNumbervb = NULL;
static size_t vmCurCpuNumbernvb = 0;
static size_t vmCurCpuNumbervblen = 0;
static struct control_agentx_vb *vmMemUnitvb = NULL;
static size_t vmMemUnitnvb = 0;
static size_t vmMemUnitvblen = 0;
static struct control_agentx_vb *vmCurMemvb = NULL;
static size_t vmCurMemnvb = 0;
static size_t vmCurMemvblen = 0;
static struct control_agentx_vb *vmMinMemvb = NULL;
static size_t vmMinMemnvb = 0;
static size_t vmMinMemvblen = 0;
static struct control_agentx_vb *vmMaxMemvb = NULL;
static size_t vmMaxMemnvb = 0;
static size_t vmMaxMemvblen = 0;
static int vmcollecting = 0;

#define VMMIB SUBAGENTX_MIB2, 236
#define VMOBJECTS VMMIB, 1
#define VMHVSOFTWARE VMOBJECTS, 1, 1
#define VMHVVERSION VMOBJECTS, 1, 2
#define VMHVOBJECTID VMOBJECTS, 1, 3
#define VMNUMBER VMOBJECTS, 2
#define VMENTRY VMOBJECTS, 4, 1
#define VMINDEX VMENTRY, 1
#define VMNAME VMENTRY, 2
#define VMUUID VMENTRY, 3
#define VMOSTYPE VMENTRY, 4
#define VMADMINSTATE VMENTRY, 5
#define VMOPERSTATE VMENTRY, 6
#define VMAUTOSTART VMENTRY, 7
#define VMPERSISTENT VMENTRY, 8
#define VMCURCPUNUMBER VMENTRY, 9
#define VMMINCPUNUMBER VMENTRY, 10
#define VMMAXCPUNUMBER VMENTRY, 11
#define VMMEMUNITS VMENTRY, 12
#define VMCURMEM VMENTRY, 13
#define VMMINMEM VMENTRY, 14
#define VMMAXMEM VMENTRY, 15

#define VIRTUALMACHINEADMINSTATE_RUNNING 1
#define VIRTUALMACHINEADMINSTATE_SUSPENDED 2
#define VIRTUALMACHINEADMINSTATE_PAUSED 3
#define VIRTUALMACHINEADMINSTATE_SHUTDOWN 4

#define VIRTUALMACHINEOPERSTATE_UNKNOWN 1
#define VIRTUALMACHINEOPERSTATE_OTHER 2
#define VIRTUALMACHINEOPERSTATE_PREPARING 3
#define VIRTUALMACHINEOPERSTATE_RUNNING 4
#define VIRTUALMACHINEOPERSTATE_SUSPENDING 5
#define VIRTUALMACHINEOPERSTATE_SUSPENDED 6
#define VIRTUALMACHINEOPERSTATE_RESUMING 7
#define VIRTUALMACHINEOPERSTATE_PAUSED 8
#define VIRTUALMACHINEOPERSTATE_MIGRATING 9
#define VIRTUALMACHINEOPERSTATE_SHUTTINGDOWN 10
#define VIRTUALMACHINEOPERSTATE_SHUTDOWN 11
#define VIRTUALMACHINEOPERSTATE_CRASHED 12

#define VIRTUALMACHINEAUTOSTART_UNKNOWN 1
#define VIRTUALMACHINEAUTOSTART_ENABLED 2
#define VIRTUALMACHINEAUTOSTART_DISABLED 3

#define VIRTUALMACHINEPERSISTENT_UNKNOWN 1
#define VIRTUALMACHINEPERSISTENT_PERSISTENT 2
#define VIRTUALMACHINEPERSISTENT_TRANSIENT 3

void
control_agentx(struct privsep *ps, struct agentx *env)
{
	static char curpath[sizeof(((struct sockaddr_un *)0)->sun_path)];
	static char curcontext[50];
	char *context = env->context[0] == '\0' ? NULL : env->context;
	int changed = 0;

	agentx = env;
	aps = ps;

	subagentx_log_fatal = fatalx;
	subagentx_log_warn = log_warnx;
	subagentx_log_info = log_info;
	subagentx_log_debug = log_debug;

	if (!agentx->enabled) {
		if (sa != NULL) {
			subagentx_free(sa);
			curpath[0] = '\0';
			curcontext[0] = '\0';
			sa = NULL;
		}
		return;
	}

	if (strcmp(curpath, agentx->path) != 0) {
		if (sa != NULL)
			subagentx_free(sa);
		if ((sa = subagentx(control_agentx_nofd, NULL)) == NULL)
			fatal("Can't setup agentx");
		if ((sas = subagentx_session(sa, NULL, 0, "vmd", 0)) == NULL)
			fatal("Can't setup agentx session");
		(void) strlcpy(curpath, agentx->path, sizeof(curpath));
		curcontext[0] = '\0';
		changed = 1;
	}

	if (strcmp(curcontext, agentx->context) != 0 || changed) {
		if (!changed)
			subagentx_context_free(sac);
		if ((sac = subagentx_context(sas, context)) == NULL)
			fatal("Can't setup agentx context");
		changed = 1;
	}

	if (!changed)
		return;

	if ((vmMIB = subagentx_region(sac,
	    SUBAGENTX_OID(VMMIB), 1)) == NULL)
		fatal("subagentx_region vmMIB");
	
	if ((vmIndex = subagentx_index_integer_dynamic(vmMIB,
	    SUBAGENTX_OID(VMINDEX))) == NULL)
		fatal("subagentx_index_integer_dynamic");

        if ((subagentx_object(vmMIB, SUBAGENTX_OID(VMHVSOFTWARE), NULL, 0, 0,
	    control_agentx_vmHvSoftware)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMHVVERSION), NULL, 0, 0,
	    control_agentx_vmHvVersion)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMHVOBJECTID), NULL, 0, 0,
	    control_agentx_vmHvObjectID)) == NULL ||
            (vmNumber = subagentx_object(vmMIB, SUBAGENTX_OID(VMNUMBER), NULL,
	    0, 0, control_agentx_vmNumber)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMNAME),
	    &vmIndex, 1, 0, control_agentx_vmName)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMUUID), &vmIndex, 1, 0,
	    control_agentx_emptystring_vm)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMOSTYPE), &vmIndex, 1, 0,
	    control_agentx_emptystring_vm)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMADMINSTATE),
	    &vmIndex, 1, 0, control_agentx_vmAdminState)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMOPERSTATE),
	    &vmIndex, 1, 0, control_agentx_vmOperState)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMAUTOSTART),
	    &vmIndex, 1, 0, control_agentx_vmAutoStart)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMPERSISTENT),
	    &vmIndex, 1, 0, control_agentx_vmPersistent)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMCURCPUNUMBER),
	    &vmIndex, 1, 0, control_agentx_vmCurCpuNumber)) == NULL ||
	/* XXX Should be vmMinCpuNumber, but we don't support that */
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMMINCPUNUMBER),
	    &vmIndex, 1, 0, control_agentx_vmCurCpuNumber)) == NULL ||
	/* XXX Should be vmMaxCpuNumber, but we don't support that */
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMMAXCPUNUMBER),
	    &vmIndex, 1, 0, control_agentx_vmCurCpuNumber)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMMEMUNITS),
	    &vmIndex, 1, 0, control_agentx_vmMemUnit)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMCURMEM),
	    &vmIndex, 1, 0, control_agentx_vmCurMem)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMMINMEM),
	    &vmIndex, 1, 0, control_agentx_vmMinMem)) == NULL ||
            (subagentx_object(vmMIB, SUBAGENTX_OID(VMMAXMEM),
	    &vmIndex, 1, 0, control_agentx_vmMaxMem)) == NULL)
                fatal("subagentx_object");
}

static void
control_agentx_nofd(struct subagentx *lsa, void *cookie, int close)
{
	event_del(&rev);
	if (!close)
		control_agentx_tryconnect(-1, 0, lsa);
}

static void
control_agentx_tryconnect(int fd, short event, void *cookie)
{
	sa = cookie;

	proc_compose(aps, PROC_PARENT, IMSG_VMDOP_AGENTXFD, NULL, 0);
}

void
control_agentx_connect(int fd)
{
	struct timeval timeout = {3, 0};

	if (sa == NULL)
		return;

	if (fd == -1) {
		evtimer_set(&connev, control_agentx_tryconnect, sa);
		evtimer_add(&connev, &timeout);
		return;
	}

	subagentx_connect(sa, fd);

	event_set(&rev, fd, EV_READ|EV_PERSIST, control_agentx_read, sa);
	event_add(&rev, NULL);
}

static void
control_agentx_read(int fd, short event, void *cookie)
{
	struct subagentx *lsa = cookie;

	subagentx_read(lsa);
}

static int
control_agentx_sortvir(const void *c1, const void *c2)
{
	const struct vmop_info_result *v1 = c1, *v2 = c2;

	return v1->vir_info.vir_id < v2->vir_info.vir_id ? -1 :
	    v1->vir_info.vir_id > v2->vir_info.vir_id;
}

/* XXX This needs more scrutiny from someone who knows vmd */
static int
control_agentx_adminstate(int mask)
{
	if (mask & VM_STATE_PAUSED)
                return VIRTUALMACHINEADMINSTATE_PAUSED;
        else if (mask & VM_STATE_RUNNING)
                return VIRTUALMACHINEADMINSTATE_RUNNING;
        else if (mask & VM_STATE_SHUTDOWN)
                return VIRTUALMACHINEADMINSTATE_SHUTDOWN;
        /* Presence of absence of other flags */
        else if (!mask || (mask & VM_STATE_DISABLED))
                return VIRTUALMACHINEADMINSTATE_SHUTDOWN;

        return VIRTUALMACHINEADMINSTATE_SHUTDOWN;
}

/* XXX This needs more scrutiny from someone who knows vmd */
static int
control_agentx_operstate(int mask)
{
	if (mask & VM_STATE_PAUSED)
                return VIRTUALMACHINEOPERSTATE_PAUSED;
        else if (mask & VM_STATE_RUNNING)
                return VIRTUALMACHINEOPERSTATE_RUNNING;
        else if (mask & VM_STATE_SHUTDOWN)
                return VIRTUALMACHINEOPERSTATE_SHUTDOWN;
        /* Presence of absence of other flags */
        else if (!mask || (mask & VM_STATE_DISABLED))
                return VIRTUALMACHINEOPERSTATE_SHUTDOWN;

        return VIRTUALMACHINEOPERSTATE_SHUTDOWN;
}

void
control_agentx_dispatch_vmd(struct imsg *imsg)
{
	static struct vmop_info_result *vir = NULL;
	struct vmop_info_result *tvir;
	static uint32_t nvir = 0;
	static size_t virlen = 0;
	static int error = 0;
	size_t i;

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_GET_INFO_VM_DATA:
		if (error)
			break;
		if (nvir + 1 > virlen) {
			tvir = reallocarray(vir, virlen + 10, sizeof(*vir));
			if (tvir == NULL) {
				log_warn("%s: Couldn't dispatch vm information",
				    __func__);
				error = 1;
				break;
			}
			vir = tvir;
		}
                memcpy(&(vir[nvir++]), imsg->data, sizeof(vir[nvir]));
		break;
	case IMSG_VMDOP_GET_INFO_VM_END_DATA:
		vmcollecting = 0;
		if (error) {
			for (i = 0; i < vmNumbernvb; i++)
				subagentx_varbind_error(vmNumbervb[i].cav_vb);
			vmNumbernvb = 0;
			for (i = 0; i < vmEmptystringnvb; i++)
				subagentx_varbind_error(
				    vmEmptystringvb[i].cav_vb);
			vmEmptystringnvb = 0;
			for (i = 0; i < vmNamenvb; i++)
				subagentx_varbind_error(vmNamevb[i].cav_vb);
			vmNamenvb = 0;
			for (i = 0; i < vmAdminStatenvb; i++)
				subagentx_varbind_error(
				    vmAdminStatevb[i].cav_vb);
			vmAdminStatenvb = 0;
			for (i = 0; i < vmOperStatenvb; i++)
				subagentx_varbind_error(
				    vmOperStatevb[i].cav_vb);
			vmOperStatenvb = 0;
			for (i = 0; i < vmAutoStartnvb; i++)
				subagentx_varbind_error(
				    vmAutoStartvb[i].cav_vb);
			vmAutoStartnvb = 0;
			for (i = 0; i < vmPersistentnvb; i++)
				subagentx_varbind_error(
				    vmPersistentvb[i].cav_vb);
			vmPersistentnvb = 0;
			for (i = 0; i < vmCurCpuNumbernvb; i++)
				subagentx_varbind_error(
				    vmCurCpuNumbervb[i].cav_vb);
			vmCurCpuNumbernvb = 0;
			for (i = 0; i < vmMemUnitnvb; i++)
				subagentx_varbind_error(vmMemUnitvb[i].cav_vb);
			vmMemUnitnvb = 0;
			for (i = 0; i < vmCurMemnvb; i++)
				subagentx_varbind_error(vmCurMemvb[i].cav_vb);
			vmCurMemnvb = 0;
			for (i = 0; i < vmMinMemnvb; i++)
				subagentx_varbind_error(vmMinMemvb[i].cav_vb);
			vmMinMemnvb = 0;
			for (i = 0; i < vmMaxMemnvb; i++)
				subagentx_varbind_error(vmMaxMemvb[i].cav_vb);
			vmMaxMemnvb = 0;
			error = 0;
			nvir = 0;
			return;
		}

		qsort(vir, nvir, sizeof(*vir), control_agentx_sortvir);
		for (i = 0; i < vmNumbernvb; i++)
			subagentx_varbind_integer(vmNumbervb[i].cav_vb, nvir);
		vmNumbernvb = 0;
		for (i = 0; i < vmEmptystringnvb; i++)
			control_agentx_vm_finalize(&(vmEmptystringvb[i]), vir,
			    nvir, control_agentx_emptystring_vm_finalize);
		vmEmptystringnvb = 0;
		for (i = 0; i < vmNamenvb; i++)
			control_agentx_vm_finalize(&(vmNamevb[i]), vir,
			    nvir, control_agentx_vmName_finalize);
		vmNamenvb = 0;
		for (i = 0; i < vmAdminStatenvb; i++)
			control_agentx_vm_finalize(&(vmAdminStatevb[i]),
			    vir, nvir, control_agentx_vmAdminState_finalize);
		vmAdminStatenvb = 0;
		for (i = 0; i < vmOperStatenvb; i++)
			control_agentx_vm_finalize(&(vmOperStatevb[i]),
			    vir, nvir, control_agentx_vmOperState_finalize);
		vmOperStatenvb = 0;
		for (i = 0; i < vmAutoStartnvb; i++)
			control_agentx_vm_finalize(&(vmAutoStartvb[i]),
			    vir, nvir, control_agentx_vmAutoStart_finalize);
		vmAutoStartnvb = 0;
		for (i = 0; i < vmPersistentnvb; i++)
			control_agentx_vm_finalize(&(vmPersistentvb[i]),
			    vir, nvir, control_agentx_vmPersistent_finalize);
		vmPersistentnvb = 0;
		for (i = 0; i < vmCurCpuNumbernvb; i++)
			control_agentx_vm_finalize(&(vmCurCpuNumbervb[i]),
			    vir, nvir, control_agentx_vmCurCpuNumber_finalize);
		vmCurCpuNumbernvb = 0;
		for (i = 0; i < vmMemUnitnvb; i++)
			control_agentx_vm_finalize(&(vmMemUnitvb[i]),
			    vir, nvir, control_agentx_vmMemUnit_finalize);
		vmMemUnitnvb = 0;
		for (i = 0; i < vmCurMemnvb; i++)
			control_agentx_vm_finalize(&(vmCurMemvb[i]),
			    vir, nvir, control_agentx_vmCurMem_finalize);
		vmCurMemnvb = 0;
		for (i = 0; i < vmMinMemnvb; i++)
			control_agentx_vm_finalize(&(vmMinMemvb[i]),
			    vir, nvir, control_agentx_vmMinMem_finalize);
		vmMinMemnvb = 0;
		for (i = 0; i < vmMaxMemnvb; i++)
			control_agentx_vm_finalize(&(vmMaxMemvb[i]),
			    vir, nvir, control_agentx_vmMaxMem_finalize);
		vmMaxMemnvb = 0;
		nvir = 0;
		break;
	default:
		fatalx("%s: error handling imsg %d", __func__, imsg->hdr.type);
	}
}

static void
control_agentx_vmHvSoftware(struct subagentx_varbind *vb)
{
	subagentx_varbind_string(vb, "OpenBSD VMM");
}

static void
control_agentx_vmHvVersion(struct subagentx_varbind *vb)
{
	int osversid[] = {CTL_KERN, KERN_OSRELEASE};
	static char osvers[10] = "";
	size_t osverslen;

	if (osvers[0] == '\0') {
		osverslen = sizeof(osvers);
		if (sysctl(osversid, 2, osvers, &osverslen, NULL,
		    0) == -1) {
			log_warn("Failed vmHvVersion sysctl");
			subagentx_varbind_string(vb, "unknown");
			return;
		}
		if (osverslen >= sizeof(osvers))
			osverslen = sizeof(osvers) - 1;
		osvers[osverslen] = '\0';
	}
	subagentx_varbind_string(vb, osvers);
}

static void
control_agentx_vmHvObjectID(struct subagentx_varbind *vb)
{
	subagentx_varbind_oid(vb, NULL, 0);
}

static void
control_agentx_get_vmInfo(struct control_agentx_vb **vblist, size_t *nvb,
    size_t *vblen, struct subagentx_varbind *vb)
{
	extern struct vmd *env;
	struct control_agentx_vb *tvb;

	if (*nvb + 1 > *vblen) {
		if ((tvb = reallocarray(*vblist, (*vblen) + 10,
		    sizeof(**vblist))) == NULL) {
			log_warn("%s: Couldn't retrieve vm information",
			    __func__);
			subagentx_varbind_error(vb);
			return;
		}
		*vblist = tvb;
		*vblen += 10;
	}

	if (!vmcollecting) {
		if (proc_compose_imsg(&(env->vmd_ps), PROC_PARENT, UINT32_MAX,
		    IMSG_VMDOP_GET_INFO_VM_REQUEST, CONTROL_AGENTX_PEERID, -1,
		    NULL, 0) == -1) {
			log_warn("%s: Couldn't retrieve vm information",
			    __func__);
			subagentx_varbind_error(vb);
			return;
		}
		vmcollecting = 1;
	}

	(*vblist)[*nvb].cav_vb = vb;
	if (subagentx_varbind_get_object(vb) != vmNumber)
		(*vblist)[(*nvb)].cav_idx = vmIndex;
	(*nvb)++;

}

static void
control_agentx_vmNumber(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmNumbervb, &vmNumbernvb, &vmNumbervblen,
	    vb);
}

static void
control_agentx_emptystring_vm(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmEmptystringvb, &vmEmptystringnvb,
	   &vmEmptystringvblen, vb);
}

static void
control_agentx_vmName(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmNamevb, &vmNamenvb, &vmNamevblen, vb);
}

static void
control_agentx_vmAdminState(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmAdminStatevb, &vmAdminStatenvb,
	    &vmAdminStatevblen, vb);
}

static void
control_agentx_vmOperState(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmOperStatevb, &vmOperStatenvb,
	    &vmOperStatevblen, vb);
}

static void
control_agentx_vmAutoStart(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmAutoStartvb, &vmAutoStartnvb,
	    &vmAutoStartvblen, vb);
}

static void
control_agentx_vmPersistent(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmPersistentvb, &vmPersistentnvb,
	    &vmPersistentvblen, vb);
}

static void
control_agentx_vmCurCpuNumber(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmCurCpuNumbervb, &vmCurCpuNumbernvb,
	    &vmCurCpuNumbervblen, vb);
}

static void
control_agentx_vmMemUnit(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmMemUnitvb, &vmMemUnitnvb, &vmMemUnitvblen,
	    vb);
}

static void
control_agentx_vmCurMem(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmCurMemvb, &vmCurMemnvb, &vmCurMemvblen,
	    vb);
}

static void
control_agentx_vmMinMem(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmMinMemvb, &vmMinMemnvb, &vmMinMemvblen,
	    vb);
}

static void
control_agentx_vmMaxMem(struct subagentx_varbind *vb)
{
	control_agentx_get_vmInfo(&vmMaxMemvb, &vmMaxMemnvb, &vmMaxMemvblen,
	    vb);
}

static void
control_agentx_vm_finalize(struct control_agentx_vb *vb,
    struct vmop_info_result *vir, uint32_t nvir,
    void (*match)(struct subagentx_varbind *, struct vmop_info_result *))
{
	uint32_t idx;
	uint32_t i;

	idx = subagentx_varbind_get_index_integer(vb->cav_vb, vb->cav_idx);
	switch (subagentx_varbind_request(vb->cav_vb)) {
	case SUBAGENTX_REQUEST_TYPE_GET:
		for (i = 0; i < nvir; i++) {
			if (vir[i].vir_info.vir_id == idx) {
				match(vb->cav_vb, &(vir[i]));
				return;
			}
		}
		break;
	case SUBAGENTX_REQUEST_TYPE_GETNEXT:
		for (i = 0; i < nvir; i++) {
			if (vir[i].vir_info.vir_id > idx) {
				subagentx_varbind_set_index_integer(vb->cav_vb,
				    vb->cav_idx, vir[i].vir_info.vir_id);
				match(vb->cav_vb, &(vir[i]));
				return;
			}
		}
		break;
	case SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE:
		for (i = 0; i < nvir; i++) {
			if (vir[i].vir_info.vir_id >= idx) {
				subagentx_varbind_set_index_integer(vb->cav_vb,
				    vb->cav_idx, vir[i].vir_info.vir_id);
				match(vb->cav_vb, &(vir[i]));
				return;
			}
		}
		break;
	}
	subagentx_varbind_notfound(vb->cav_vb);
	return;
}

static void
control_agentx_emptystring_vm_finalize(struct subagentx_varbind *vb,
    struct vmop_info_result *vir)
{
	subagentx_varbind_string(vb, "");
}

static void
control_agentx_vmName_finalize(struct subagentx_varbind *vb,
    struct vmop_info_result *vir)
{
	subagentx_varbind_string(vb, vir->vir_info.vir_name);
}

static void
control_agentx_vmAdminState_finalize(struct subagentx_varbind *vb,
    struct vmop_info_result *vir)
{
	subagentx_varbind_integer(vb,
	    control_agentx_adminstate(vir->vir_state));
}

static void
control_agentx_vmOperState_finalize(struct subagentx_varbind *vb,
    struct vmop_info_result *vir)
{
	subagentx_varbind_integer(vb, control_agentx_operstate(vir->vir_state));
}

static void
control_agentx_vmAutoStart_finalize(struct subagentx_varbind *vb,
    struct vmop_info_result *vir)
{
	subagentx_varbind_integer(vb,
	    vir->vir_state & VM_STATE_DISABLED ?
	    VIRTUALMACHINEAUTOSTART_DISABLED : VIRTUALMACHINEAUTOSTART_ENABLED);
}

/* XXX We can dynamically create vm's but I don't know how to differentiate */
static void
control_agentx_vmPersistent_finalize(struct subagentx_varbind *vb,
    struct vmop_info_result *vir)
{
	subagentx_varbind_integer(vb, 1);
}

static void
control_agentx_vmCurCpuNumber_finalize(struct subagentx_varbind *vb,
    struct vmop_info_result *vir)
{
	subagentx_varbind_integer(vb, vir->vir_info.vir_ncpus);
}

static void
control_agentx_vmMemUnit_finalize(struct subagentx_varbind *vb,
    struct vmop_info_result *vir)
{
	subagentx_varbind_integer(vb, 1024 * 1024);
}

static void
control_agentx_vmCurMem_finalize(struct subagentx_varbind *vb,
    struct vmop_info_result *vir)
{
	subagentx_varbind_integer(vb, vir->vir_info.vir_used_size / 1024 /1024);
}

static void
control_agentx_vmMinMem_finalize(struct subagentx_varbind *vb,
    struct vmop_info_result *vir)
{
	subagentx_varbind_integer(vb, -1);
}

static void
control_agentx_vmMaxMem_finalize(struct subagentx_varbind *vb,
    struct vmop_info_result *vir)
{
	subagentx_varbind_integer(vb, vir->vir_info.vir_memory_size);
}
