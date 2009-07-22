/*	$OpenBSD: arcbios.c,v 1.12 2009/07/22 20:23:44 miod Exp $	*/
/*-
 * Copyright (c) 1996 M. Warner Losh.  All rights reserved.
 * Copyright (c) 1996-2004 Opsycon AB.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <lib/libkern/libkern.h>

#include <mips64/arcbios.h>
#include <mips64/archtype.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/mnode.h>

#include <stand.h>

static int	bios_is_32bit;

u_int	kl_n_shift = 32;
int	arcbios_init(void);
const char *boot_get_path_component(const char *, char *, int *);
const char *boot_getnr(const char *, int *);

static const struct systypes {
	char *sys_name;
	int  sys_ip;
} sys_types[] = {
    { "SGI-IP30", 30 },
    { "SGI-IP32", 32 }
};

#define KNOWNSYSTEMS (nitems(sys_types))

/*
 * ARCBios trampoline code.
 */
#define ARC_Call(Name,Offset)	\
__asm__("\n"			\
"	.text\n"		\
"	.ent	" #Name "\n"	\
"	.align	3\n"		\
"	.set	noreorder\n"	\
"	.globl	" #Name "\n"	\
#Name":\n"			\
"	lw	$2, bios_is_32bit\n"\
"	beqz	$2, 1f\n"	\
"	nop\n"			\
"       lw      $2, 0xffffffff80001020\n"\
"       lw      $2," #Offset "($2)\n"\
"	jr	$2\n"		\
"	nop\n"			\
"1:\n"				\
"       ld      $2, 0xffffffff80001040\n"\
"	ld	$2, 2*" #Offset "($2)\n"\
"	jr	$2\n"		\
"	nop\n"			\
"	.end	" #Name "\n"	);

ARC_Call(Bios_Load,			0x00);
ARC_Call(Bios_Invoke,			0x04);
ARC_Call(Bios_Execute,			0x08);
ARC_Call(Bios_Halt,			0x0c);
ARC_Call(Bios_PowerDown,		0x10);
ARC_Call(Bios_Restart,			0x14);
ARC_Call(Bios_Reboot,			0x18);
ARC_Call(Bios_EnterInteractiveMode,	0x1c);
ARC_Call(Bios_Unused1,			0x20);
ARC_Call(Bios_GetPeer,			0x24);
ARC_Call(Bios_GetChild,			0x28);
ARC_Call(Bios_GetParent,		0x2c);
ARC_Call(Bios_GetConfigurationData,	0x30);
ARC_Call(Bios_AddChild,			0x34);
ARC_Call(Bios_DeleteComponent,		0x38);
ARC_Call(Bios_GetComponent,		0x3c);
ARC_Call(Bios_SaveConfiguration,	0x40);
ARC_Call(Bios_GetSystemId,		0x44);
ARC_Call(Bios_GetMemoryDescriptor,	0x48);
ARC_Call(Bios_Unused2,			0x4c);
ARC_Call(Bios_GetTime,			0x50);
ARC_Call(Bios_GetRelativeTime,		0x54);
ARC_Call(Bios_GetDirectoryEntry,	0x58);
ARC_Call(Bios_Open,			0x5c);
ARC_Call(Bios_Close,			0x60);
ARC_Call(Bios_Read,			0x64);
ARC_Call(Bios_GetReadStatus,		0x68);
ARC_Call(Bios_Write,			0x6c);
ARC_Call(Bios_Seek,			0x70);
ARC_Call(Bios_Mount,			0x74);
ARC_Call(Bios_GetEnvironmentVariable,	0x78);
ARC_Call(Bios_SetEnvironmentVariable,	0x7c);
ARC_Call(Bios_GetFileInformation,	0x80);
ARC_Call(Bios_SetFileInformation,	0x84);
ARC_Call(Bios_FlushAllCaches,		0x88);
ARC_Call(Bios_TestUnicodeCharacter,	0x8c);
ARC_Call(Bios_GetDisplayStatus,		0x90);

/*
 * Simple getchar/putchar interface.
 */

int
getchar()
{
	char buf[4];
	long cnt;

	if (Bios_Read(0, &buf[0], 1, &cnt) != 0)
		return (-1);

	return (buf[0] & 255);
}

void
putchar(int c)
{
	char buf[4];
	long cnt;

	if (c == '\n') {
		buf[0] = '\r';
		buf[1] = c;
		cnt = 2;
	} else {
		buf[0] = c;
		cnt = 1;
	}

	Bios_Write(1, &buf[0], cnt, &cnt);
}

/*
 * Identify ARCBios type.
 */
int
arcbios_init()
{
	arc_config_t *cf;
	arc_sid_t *sid;
	char *sysid = NULL;
	int sysid_len;
	int i;

	/*
	 * Figure out if this is an ARCBios machine and if it is, see if we're
	 * dealing with a 32 or 64 bit version.
	 */
	if ((ArcBiosBase32->magic == ARC_PARAM_BLK_MAGIC) ||
	    (ArcBiosBase32->magic == ARC_PARAM_BLK_MAGIC_BUG)) {
		bios_is_32bit = 1;
	} else if ((ArcBiosBase64->magic == ARC_PARAM_BLK_MAGIC) ||
	    (ArcBiosBase64->magic == ARC_PARAM_BLK_MAGIC_BUG)) {
		bios_is_32bit = 0;
	}

	/*
	 * Minimal system identification.
	 */
	sid = (arc_sid_t *)Bios_GetSystemId();
	cf = (arc_config_t *)Bios_GetChild(NULL);
	if (cf != NULL) {
		if (bios_is_32bit) {
			sysid = (char *)(long)cf->id;
			sysid_len = cf->id_len;
		} else {
			sysid = (char *)((arc_config64_t *)cf)->id;
			sysid_len = ((arc_config64_t *)cf)->id_len;
		}

		if (sysid_len > 0 && sysid != NULL) {
			sysid_len--;
			for (i = 0; i < KNOWNSYSTEMS; i++) {
				if (strlen(sys_types[i].sys_name) != sysid_len)
					continue;
				if (strncmp(sys_types[i].sys_name, sysid,
				    sysid_len) != 0)
					continue;
				return sys_types[i].sys_ip;	/* Found it. */
			}
		}
	} else {
#ifdef __LP64__
		if (IP27_KLD_KLCONFIG(0)->magic == IP27_KLDIR_MAGIC) {
			/*
			 * If we find a kldir assume IP27. Boot blocks
			 * do not need to tell IP27 and IP35 apart.
			 */
			return 27;
		}
#endif
	}

	printf("UNRECOGNIZED SYSTEM '%s' VENDOR '%s' PRODUCT '%s'\n",
	    cf == NULL || sysid == NULL ? "(null)" : sysid,
	    sid->vendor, sid->prodid);
	printf("Halting system!\n");
	Bios_Halt();
	printf("Halting failed, use manual reset!\n");
	for (;;) ;
}

/*
 *  Decompose the device pathname and find driver.
 *  Returns pointer to remaining filename path in file.
 */
int
devopen(struct open_file *f, const char *fname, char **file)
{
	const char *cp, *ncp, *ecp;
	struct devsw *dp;
	int partition = 0;
	char namebuf[256];
	char devname[32];
	int rc, i, n;

	ecp = cp = fname;

	/*
	 * Scan the component list and find device and partition.
	 */
	if (strncmp(cp, "dksc(", 5) == 0) {
		strncpy(devname, "scsi", sizeof(devname));
		cp += 5;
		cp = boot_getnr(cp, &i);
		/* i = controller number */
		if (*cp++ == ',') {
			cp = boot_getnr(cp, &i);
			/* i = target id */
			if (*cp++ == ',') {

				memcpy(namebuf, fname, cp - fname);
				namebuf[cp - fname] = '\0';
				strlcat(namebuf, "0)", sizeof namebuf);

				cp = boot_getnr(cp, &i);
				partition = i;
				cp++;	/* skip final ) */
			}
		}
	} else {
		ncp = boot_get_path_component(cp, namebuf, &i);
		while (ncp != NULL) {
			if (strcmp(namebuf, "partition") == 0)
				partition = i;
			ecp = ncp;

			/* XXX Do this with a table if more devs are added. */
			if (strcmp(namebuf, "scsi") == 0)
				strncpy(devname, namebuf, sizeof(devname)); 

			cp = ncp;
			ncp = boot_get_path_component(cp, namebuf, &i);
		}

		memcpy(namebuf, fname, ecp - fname);
		namebuf[ecp - fname] = '\0';
	}

	/*
	 * Dig out the driver.
	 */
	dp = devsw;
	n = ndevs;
	while(n--) {
		if (strcmp(devname, dp->dv_name) == 0) {
			rc = (dp->dv_open)(f, namebuf, partition, 0);
			if (!rc) {
				f->f_dev = dp;
				if (file && *cp != '\0')
					*file = (char *)cp;
			}
			return (rc);
		}
		dp++;
	}
	return (ENXIO);
}

const char *
boot_get_path_component(const char *p, char *comp, int *no)
{
	while (*p && *p != '(')
		*comp++ = *p++;
	*comp = '\0';

	if (*p == NULL)
		return (NULL);

	*no = 0;
	p++;
	while (*p && *p != ')') {
		if (*p >= '0' && *p <= '9')
			*no = *no * 10 + *p++ - '0';
		else
			return (NULL);
	}
	return (++p);
}

const char *
boot_getnr(const char *p, int *no)
{
	*no = 0;
	while (*p >= '0' && *p <= '9')
		*no = *no * 10 + *p++ - '0';
	return p;
}
