/*	$NetBSD: adbsys.c,v 1.15 1995/09/04 02:50:57 briggs Exp $	*/

/*-
 * Copyright (C) 1994	Bradley A. Grantham
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
 *	This product includes software developed by Bradley A. Grantham.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <machine/adbsys.h>

#include "adbvar.h"
#include "../mac68k/via.h"
#include "../mac68k/macrom.h"

/* from adb.c */
void    adb_processevent(adb_event_t * event);

extern void adb_jadbproc(void);

void 
adb_complete(buffer, data_area, adb_command)
	caddr_t buffer;
	caddr_t data_area;
	int adb_command;
{
	register int i;
	register char *sbuf, *dbuf;
	adb_event_t event;
	ADBDataBlock adbdata;
	int adbaddr;
	int error;

#if defined(MRG_DEBUG)
	printf("adb: transaction completion\n");
#endif

	adbaddr = (adb_command & 0xf0) >> 4;
	error = GetADBInfo(&adbdata, adbaddr);
#if defined(MRG_DEBUG)
	printf("adb: GetADBInfo returned %d\n", error);
#endif

	event.addr = adbaddr;
	event.hand_id = adbdata.devType;
	event.def_addr = adbdata.origADBAddr;
#if defined(MRG_DEBUG)
	printf("adb: from %d at %d (org %d) %d:", event.addr,
		event.hand_id, event.def_addr, buffer[0]);
	for (i = 1; i <= buffer[0]; i++)
		printf(" %x", buffer[i]);
	printf("\n");
#endif

	i = event.byte_count = buffer[0];
	sbuf = &buffer[1];
	dbuf = &event.bytes[0];
	while (i--)
		*dbuf++ = *sbuf++;

	microtime(&event.timestamp);

	adb_processevent(&event);
}

static int extdms_done;

/*
 * initialize extended mouse - probes devices as
 * described in _Inside Macintosh, Devices_.
 */
void
extdms_init()
{
	ADBDataBlock adbdata;
	int totaladbs;
	int adbindex, adbaddr;
	short cmd;
	char buffer[9];
	void extdms_complete();

	totaladbs = CountADBs();
	for (adbindex = 1; adbindex <= totaladbs; adbindex++) {
		/* Get the ADB information */
		adbaddr = GetIndADB(&adbdata, adbindex);
		if (adbdata.origADBAddr == ADBADDR_MS &&
		    (adbdata.devType == 1 || adbdata.devType == 2)) {
			/* found a mouse */
			cmd = ((adbaddr << 4) & 0xf0) | 0x3;

			extdms_done = 0;
			cmd = (cmd & 0xf3) | 0x0c; /* talk command */
			ADBOp((Ptr)buffer, (Ptr)extdms_complete,
			      (Ptr)&extdms_done, cmd);
			while (!extdms_done)
				/* busy wait until done */;

			buffer[2] = '\004'; /* make handler ID 4 */
			extdms_done = 0;
			cmd = (cmd & 0xf3) | 0x08; /* listen command */
			ADBOp((Ptr)buffer, (Ptr)extdms_complete,
			      (Ptr)&extdms_done, cmd);
			while (!extdms_done)
				/* busy wait until done */;
		}
	}
}

void 
adb_init()
{
	ADBDataBlock adbdata;
	ADBSetInfoBlock adbinfo;
	int i, s;
	int totaladbs;
	int adbindex, adbaddr;
	int error;
	char buffer[9];
	void extdms_complete();

	if (!mrg_romready()) {
		printf("adb: no ROM ADB driver in this kernel for this machine\n");
		return;
	}
	printf("adb: bus subsystem\n");
#if defined(MRG_DEBUG)
	printf("adb: call mrg_initadbintr\n");
#endif

	mrg_initadbintr();	/* Mac ROM Glue okay to do ROM intr */
#if defined(MRG_DEBUG)
	printf("adb: returned from mrg_initadbintr\n");
#endif

	/* ADBReInit pre/post-processing */
	JADBProc = adb_jadbproc;

	/* Initialize ADB */
#if defined(MRG_DEBUG)
	printf("adb: calling ADBAlternateInit\n");
#endif

	ADBAlternateInit();

#if defined(MRG_DEBUG)
	printf("adb: done with ADBReInit\n");
#endif

	extdms_init();

	totaladbs = CountADBs();

	/* for each ADB device */
	for (adbindex = 1; adbindex <= totaladbs; adbindex++) {
		/* Get the ADB information */
		adbaddr = GetIndADB(&adbdata, adbindex);

		/* Print out the glory */
		printf("adb: ");
		switch (adbdata.origADBAddr) {
		case 2:
			switch (adbdata.devType) {
			case 1:
				printf("keyboard");
				break;
			case 2:
				printf("extended keyboard");
				break;
			case 12:
				printf("PowerBook keyboard");
				break;
			default:
				printf("mapped device (%d)",
				    adbdata.devType);
				break;
			}
			break;
		case 3:
			extdms_done = 0;
			/* talk register 3 */
			ADBOp((Ptr)buffer, (Ptr)extdms_complete,
			      (Ptr)&extdms_done, (adbaddr << 4) | 0xf);
			while (!extdms_done)
				/* busy-wait until done */;
			switch (buffer[2]) {
			case 1:
				printf("100 dpi mouse");
				break;
			case 2:
				printf("200 dpi mouse");
				break;
			case 4:
				extdms_done = 0;
				/* talk register 1 */
				ADBOp((Ptr)buffer, (Ptr)extdms_complete,
				      (Ptr)&extdms_done,
				      (adbaddr << 4) | 0xd);
				while (!extdms_done)
					/* busy-wait until done */;
				printf("extended mouse "
				       "<%c%c%c%c> %d-button %d dpi ",
				       buffer[1], buffer[2],
				       buffer[3], buffer[4],
				       (int)buffer[8],
				       (int)*(short *)&buffer[5]);
				if (buffer[7] == 1)
					printf("mouse");
				else if (buffer[7] == 2)
					printf("trackball");
				else
					printf("unknown device");
				break;
			default:
				printf("relative positioning device (mouse?) (%d)", adbdata.devType);
				break;
			}
			break;
		case 4:
			printf("absolute positioning device (tablet?) (%d)", adbdata.devType);
			break;
		default:
			printf("unknown type device, (def %d, handler %d)", adbdata.origADBAddr,
			    adbdata.devType);
			break;
		}
		printf(" at %d\n", adbaddr);

		/* Set completion routine to be MacBSD's */
		adbinfo.siServiceRtPtr = (Ptr) adb_asmcomplete;
		adbinfo.siDataAreaAddr = NULL;
		error = SetADBInfo(&adbinfo, adbaddr);
#if defined(MRG_DEBUG)
		printf("returned %d from SetADBInfo\n", error);
#endif
	}
}
