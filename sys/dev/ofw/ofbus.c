/*	$OpenBSD: ofbus.c,v 1.4 1998/09/09 04:25:52 rahnds Exp $	*/
/*	$NetBSD: ofbus.c,v 1.3 1996/10/13 01:38:11 christos Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <dev/ofw/openfirm.h>

int ofrprobe __P((struct device *, void *, void *));
void ofrattach __P((struct device *, struct device *, void *));
int ofbprobe __P((struct device *, void *, void *));
void ofbattach __P((struct device *, struct device *, void *));
static int ofbprint __P((void *, const char *));

struct cfattach ofbus_ca = {
	sizeof(struct device), ofbprobe, ofbattach
};

struct cfdriver ofbus_cd = {
	NULL, "ofbus", DV_DULL
};

struct cfattach ofroot_ca = {
	sizeof(struct device), ofrprobe, ofrattach
};
 
struct cfdriver ofroot_cd = {
	NULL, "ofroot", DV_DULL
};

static int
ofbprint(aux, name)
	void *aux;
	const char *name;
{
	struct ofprobe *ofp = aux;
	char child[64];
	int l;
	
	if ((l = OF_getprop(ofp->phandle, "name", child, sizeof child - 1)) < 0)
		panic("device without name?");
	if (l >= sizeof child)
		l = sizeof child - 1;
	child[l] = 0;
	
	if (name)
		printf("%s at %s", child, name);
	else
		printf(" (%s)", child);
	return UNCONF;
}

int
ofrprobe(parent, cf, aux)
	struct device *parent;
	void *cf, *aux;
{
	int node;
	struct confargs *ca = aux;
	
	if (strcmp(ca->ca_name, ofroot_cd.cd_name) != 0)
		return (0);

	return 1; 
}
void
ofrattach(parent, dev, aux)
	struct device *parent, *dev;
	void *aux;
{
	int child;
	char name[5];
	struct ofprobe *ofp = aux;
	struct ofprobe probe;
	int units;
	int node;
	char ofname[64];
	int l;
	
        if (!(node = OF_peer(0)))
                panic("No PROM root");
        probe.phandle = node;
	ofp = &probe;

	ofbprint(ofp, 0);
	printf("\n");

	if ((l = OF_getprop(ofp->phandle, "name", ofname, sizeof ofname - 1)) < 0)
	{
		/* no system name? */
	} else {
		if (l >= sizeof ofname)
			l = sizeof ofname - 1;
		ofname[l] = 0;
		systype(ofname);
	}
	ofw_intr_establish();
		

	for (child = OF_child(ofp->phandle); child; child = OF_peer(child)) {
		/*
		 * This is a hack to skip all the entries in the tree
		 * that aren't devices (packages, openfirmware etc.).
		 */
		if (OF_getprop(child, "device_type", name, sizeof name) < 0)
			continue;
		probe.phandle = child;
		probe.unit = 0;
		config_found(dev, &probe, ofbprint);
	}
}
int
ofbprobe(parent, cf, aux)
	struct device *parent;
	void *cf, *aux;
{
	struct ofprobe *ofp = aux;
	
	if (!OF_child(ofp->phandle))
		return 0;
	return 1;
}

void
ofbattach(parent, dev, aux)
	struct device *parent, *dev;
	void *aux;
{
	int child;
	char name[5];
	struct ofprobe *ofp = aux;
	struct ofprobe probe;
	int units;
	
	if (!parent)
		ofbprint(aux, 0);
		
	printf("\n");

	/*
	 * This is a hack to make the probe work on the scsi (and ide) bus.
	 * YES, I THINK IT IS A BUG IN THE OPENFIRMWARE TO NOT PROBE ALL
	 * DEVICES ON THESE BUSSES.
	 */
	units = 1;
	if (OF_getprop(ofp->phandle, "name", name, sizeof name) > 0) {
		if (!strcmp(name, "scsi"))
			units = 7; /* What about wide or hostid != 7?	XXX */
		else if (!strcmp(name, "ide"))
			units = 2;
	}
	for (child = OF_child(ofp->phandle); child; child = OF_peer(child)) {
		/*
		 * This is a hack to skip all the entries in the tree
		 * that aren't devices (packages, openfirmware etc.).
		 */
		if (OF_getprop(child, "device_type", name, sizeof name) < 0)
			continue;
		probe.phandle = child;
		for (probe.unit = 0; probe.unit < units; probe.unit++)
			config_found(dev, &probe, ofbprint);
	}
}

/*
 * Name matching routine for OpenFirmware
 */
int
ofnmmatch(cp1, cp2)
	char *cp1, *cp2;
{
	int i;
	
	for (i = 0; *cp2; i++)
		if (*cp1++ != *cp2++)
			return 0;
	return i;
}
