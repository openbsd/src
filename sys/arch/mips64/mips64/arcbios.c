/*	$OpenBSD: arcbios.c,v 1.6 2004/09/09 22:11:38 pefo Exp $	*/
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
#include <sys/systm.h>
#include <sys/proc.h>
#include <lib/libkern/libkern.h>
#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/memconf.h>
#include <machine/param.h>
#include <machine/autoconf.h>
#include <mips64/arcbios.h>
#include <mips64/archtype.h>

arc_param_blk_t *bios_base = ArcBiosBase;

extern int	physmem;		/* Total physical memory size */
extern int	rsvdmem;		/* Total reserved memory size */

void bios_configure_memory(void);
int bios_get_system_type(void);

arc_dsp_stat_t	displayinfo;		/* Save area for display status info. */

static struct systypes {
	char *sys_vend;		/* Vendor ID if name is ambigous */
	char *sys_name;		/* May be left NULL if name is sufficient */
	int  sys_type;
} sys_types[] = {
    { NULL,		"PICA-61",			ACER_PICA_61 },
    { NULL,		"NEC-R94",			ACER_PICA_61 },
    { NULL,		"DESKTECH-TYNE",		DESKSTATION_TYNE },
    { NULL,		"DESKTECH-ARCStation I",	DESKSTATION_RPC44 },
    { NULL,		"Microsoft-Jazz",		MAGNUM },
    { NULL,		"RM200PCI",			SNI_RM200 },
    { NULL,		"SGI-IP17",			SGI_CRIMSON },
    { NULL,		"SGI-IP19",			SGI_ONYX },
    { NULL,		"SGI-IP20",			SGI_INDIGO },
    { NULL,		"SGI-IP21",			SGI_POWER },
    { NULL,		"SGI-IP22",			SGI_INDY },
    { NULL,		"SGI-IP25",			SGI_POWER10 },
    { NULL,		"SGI-IP26",			SGI_POWERI },
    { NULL,		"SGI-IP32",			SGI_O2 },
};

#define KNOWNSYSTEMS (sizeof(sys_types) / sizeof(struct systypes))

/*
 *	ARC Bios trampoline code.
 */
#define ARC_Call(Name,Offset)	\
__asm__("\n"			\
"	.text\n"		\
"	.ent	" #Name "\n"	\
"	.align	3\n"		\
"	.set	noreorder\n"	\
"	.globl	" #Name "\n"	\
#Name":\n"			\
"	lw	$2, 0xffffffff80001020\n"\
"	lw	$2," #Offset "($2)\n"\
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
 *	Simple getchar/putchar interface.
 */

int
bios_getchar()
{
	char buf[4];
	int  cnt;

	if (Bios_Read(0, &buf[0], 1, &cnt) != 0)
		return(-1);
	return(buf[0] & 255);
}

void
bios_putchar(c)
char c;
{
	char buf[4];
	int  cnt;

	if (c == '\n') {
		buf[0] = '\r';
		buf[1] = c;
		cnt = 2;
		if (displayinfo.CursorYPosition < displayinfo.CursorMaxYPosition)
			displayinfo.CursorYPosition++;
	}
	else {
		buf[0] = c;
		cnt = 1;
	}
	Bios_Write(1, &buf[0], cnt, &cnt);
}

void
bios_putstring(s)
char *s;
{
	while (*s) {
		bios_putchar(*s++);
	}
}

/*
 * Get memory descriptor for the memory configuration and
 * create a layout database used by pmap init to set up
 * the memory system. Note that kernel option "MACHINE_NONCONTIG"
 * must be set for systems with non contigous physical memory.
 *
 * Concatenate obvious adjecent segments.
 */
void
bios_configure_memory()
{
	arc_mem_t *descr = 0;
	struct phys_mem_desc *m;
	vaddr_t seg_start, seg_end;
	int	i;

	descr = (arc_mem_t *)Bios_GetMemoryDescriptor(descr);
	while(descr != 0) {

		seg_start = descr->BasePage;
		seg_end = seg_start + descr->PageCount;

		switch (descr->Type) {
		case BadMemory:		/* Have no use for theese */
			break;

		case FreeMemory:
		case FreeContigous:
			physmem += descr->PageCount;
			m = 0;
			for (i = 0; i < MAXMEMSEGS; i++) {
				if (mem_layout[i].mem_last_page == 0) {
					if (m == 0)
						m = &mem_layout[i]; /* free */
				}
				else if (seg_end == mem_layout[i].mem_first_page) {
					m = &mem_layout[i];
					m->mem_first_page = seg_start;
				}
				else if (mem_layout[i].mem_last_page == seg_start) {
					m = &mem_layout[i];
					m->mem_last_page = seg_end;
				}
			}
			if (m && m->mem_first_page == 0) {
				m->mem_first_page = seg_start;
				m->mem_last_page = seg_end;
			}
			break;

		case ExeceptionBlock:
		case SystemParameterBlock:
		case FirmwareTemporary:
		case FirmwarePermanent:
			rsvdmem += descr->PageCount;
			physmem += descr->PageCount;
			break;

		case LoadedProgram:	/* Count this into total memory */
			physmem += descr->PageCount;
			break;

		default:		/* Unknown type, leave it alone... */
			break;
		}
		descr = (arc_mem_t *)Bios_GetMemoryDescriptor(descr);
	}

#ifdef DEBUG_MEM_LAYOUT
	for ( i = 0; i < MAXMEMSEGS; i++) {
		char str[100];
		if (mem_layout[i].mem_first_page) {
			snprintf(str, sizeof(str), "MEM %d, 0x%x to  0x%x\n",i,
				mem_layout[i].mem_first_page * 4096,
				mem_layout[i].mem_last_page * 4096);
			bios_putstring(str);
	    }
	}
#endif
}

/*
 * Find out system type.
 */
int
bios_get_system_type()
{
	arc_config_t	*cf;
	arc_sid_t	*sid;
	int		i;

	if ((bios_base->magic != ARC_PARAM_BLK_MAGIC) &&
	    (bios_base->magic != ARC_PARAM_BLK_MAGIC_BUG)) {
		return(-1);	/* This is not an ARC system */
	}

	sid = (arc_sid_t *)Bios_GetSystemId();
	cf = (arc_config_t *)Bios_GetChild(NULL);
	if (cf) {
		for (i = 0; i < KNOWNSYSTEMS; i++) {
			if (strcmp(sys_types[i].sys_name, (char *)(long)cf->id) != 0)
				continue;
			if (sys_types[i].sys_vend &&
			    strncmp(sys_types[i].sys_vend, sid->vendor, 8) != 0)
				continue;
			return (sys_types[i].sys_type);	/* Found it. */
		}
	}

	bios_putstring("UNIDENTIFIED SYSTEM `");
	if (cf)
		bios_putstring((char *)(long)cf->id);
	else
		bios_putstring("????????");
	bios_putstring("' VENDOR `");
	sid->vendor[8] = 0;
	bios_putstring(sid->vendor);
	bios_putstring("'. Please contact OpenBSD (www.openbsd.org).\n");
	bios_putstring("Reset system to restart!\n");
	while(1);
}

/*
 * Incomplete version of bios_ident
 */
void
bios_ident()
{
	sys_config.system_type = bios_get_system_type();
	if (sys_config.system_type < 0) {
		return;
	}
	bios_configure_memory();
#ifdef __arc__
	displayinfo = *(arc_dsp_stat_t *)Bios_GetDisplayStatus(1);
#endif
}

/*
 * Return geometry of the display. Used by pccons.c to set up the
 * display configuration.
 */
void
bios_display_info(xpos, ypos, xsize, ysize)
    int	*xpos;
    int	*ypos;
    int *xsize;
    int *ysize;
{
#ifdef __arc__
	*xpos = displayinfo.CursorXPosition;
	*ypos = displayinfo.CursorYPosition;
	*xsize = displayinfo.CursorMaxXPosition;
	*ysize = displayinfo.CursorMaxYPosition;
#endif
}

