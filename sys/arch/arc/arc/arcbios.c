/*	$OpenBSD: arcbios.c,v 1.4 1996/09/24 19:37:23 pefo Exp $	*/
/*-
 * Copyright (c) 1996 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <sys/types.h>
#include <sys/param.h>
#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/memconf.h>
#include <machine/param.h>
#include <arc/arc/arcbios.h>
#include <arc/arc/arctype.h>

arc_param_blk_t *bios_base = (arc_param_blk_t *) 0x80001000;

extern int	cputype;		/* Mother board type */
extern int	physmem;		/* Total physical memory size */

char buf[100];	/*XXX*/
arc_dsp_stat_t	displayinfo;		/* Save area for display status info. */

static struct systypes {
	char *sys_vend;		/* Vendor ID if name is ambigous */
	char *sys_name;		/* May be left NULL if name is sufficient */
	int  sys_type;
} sys_types[] = {
    { NULL,		"PICA-61",			ACER_PICA_61 },
    { NULL,		"DESKTECH-TYNE",		DESKSTATION_TYNE }, 
    { NULL,		"DESKTECH-ARCStation I",	DESKSTATION_RPC44 },
    { NULL,		"Microsoft-Jazz",		MAGNUM },
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
"	.globl	" #Name "\n" 	\
#Name":\n"			\
"	lw	$2, 0x80001020\n"\
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

bios_getchar()
{
	char buf[4];
	int  cnt;

	if(Bios_Read(0, &buf, 1, &cnt) != 0)
		return(-1);
	return(buf[0] & 255);
}

bios_putchar(c)
char c;
{
	char buf[4];
	int  cnt;

	if(c == '\n') {
		buf[0] = '\r';
		buf[1] = c;
		cnt = 2;
		if(displayinfo.CursorYPosition < displayinfo.CursorMaxYPosition)
			displayinfo.CursorYPosition++;
	}
	else {
		buf[0] = c;
		cnt = 1;
	}
	if(Bios_Write(1, &buf, cnt, &cnt) != 0)
		return(-1);
	return(0);
}

bios_putstring(s)
char *s;
{
	while(*s) {
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
bios_configure_memory()
{
	arc_mem_t *descr = 0;
	struct mem_descriptor *m;
	vm_offset_t seg_start, seg_end;
	int	i;

	descr = (arc_mem_t *)Bios_GetMemoryDescriptor(descr);
	while(descr != 0) {

		seg_start = descr->BasePage * 4096;
		seg_end = seg_start + descr->PageCount * 4096;

		switch(descr->Type) {
		case BadMemory:		/* Have no use for theese */
			break;

		case ExeceptionBlock:
		case SystemParameterBlock:
		case FreeMemory:
		case FirmwareTemporary:
		case FirmwarePermanent:
		case FreeContigous:
			physmem += descr->PageCount * 4096;
			m = 0;
			for( i = 0; i < MAXMEMSEGS; i++) {
				if(mem_layout[i].mem_size == 0) {
					if(m == 0)
						m = &mem_layout[i]; /* free */
				}
				else if(seg_end == mem_layout[i].mem_start) {
					m = &mem_layout[i];
					m->mem_start = seg_start;
					m->mem_size += seg_end - seg_start;
				}
				else if(mem_layout[i].mem_start +
				    mem_layout[i].mem_size == seg_start) {
					m = &mem_layout[i];
					m->mem_size += seg_end - seg_start;
				}
			}
			if(m && m->mem_size == 0) {
				m->mem_start = seg_start;
				m->mem_size = seg_end - seg_start;
			}
			break;

		case LoadedProgram:	/* This is the loaded kernel */
			physmem += descr->PageCount * 4096;
			break;

		default:		/* Unknown type, leave it alone... */
			break;
		}
		descr = (arc_mem_t *)Bios_GetMemoryDescriptor(descr);
	}

#if 0
	for( i = 0; i < MAXMEMSEGS; i++) {
	    if(mem_layout[i].mem_size) {
		sprintf(buf, "MEM %d, 0x%x, 0x%x\n",i,
			mem_layout[i].mem_start,
			mem_layout[i].mem_size);
		bios_putstring(buf);
	    }
	}
#endif
}

/*
 * Find out system type.
 */
int
get_cpu_type()
{
	arc_config_t	*cf;
	arc_sid_t	*sid;
	int		i;

	sid = (arc_sid_t *)Bios_GetSystemId();
	cf = (arc_config_t *)Bios_GetChild(NULL);
	if(cf) {
		for(i = 0; i < KNOWNSYSTEMS; i++) {
			if(strcmp(sys_types[i].sys_name, cf->id) != 0)
				continue;
			if(sys_types[i].sys_vend &&
			    strncmp(sys_types[i].sys_vend, sid->vendor, 8) != 0)
				continue;
			return(sys_types[i].sys_type);	/* Found it. */
		}
	}

	bios_putstring("UNIDENTIFIED ARC SYSTEM `");
	if(cf)
		bios_putstring(cf->id);
	else
		bios_putstring("????????");
	bios_putstring("' VENDOR `");
	sid->vendor[8] = 0;
	bios_putstring("sid->vendor");
	bios_putstring("'. Please contact OpenBSD (www.openbsd.org).\n");
	while(1);
}

/*
 * Incomplete version of bios_ident
 */
void
bios_ident()
{
	cputype = get_cpu_type();
	bios_configure_memory();
	displayinfo = *(arc_dsp_stat_t *)Bios_GetDisplayStatus(1);
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
	*xpos = displayinfo.CursorXPosition;
	*ypos = displayinfo.CursorYPosition;
	*xsize = displayinfo.CursorMaxXPosition;
	*ysize = displayinfo.CursorMaxYPosition;
}

