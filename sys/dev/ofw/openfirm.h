/*	$NetBSD: openfirm.h,v 1.1 1996/09/30 16:35:10 ws Exp $	*/

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
/*
 * Prototypes for OpenFirmware Interface Routines
 */

#include <sys/param.h>
#include <sys/device.h>

int openfirmware __P((void *));

extern char *OF_buf;

int OF_peer __P((int phandle));
int OF_child __P((int phandle));
int OF_parent __P((int phandle));
int OF_instance_to_package __P((int ihandle));
int OF_getprop __P((int handle, char *prop, void *buf, int buflen));
int OF_finddevice __P((char *name));
int OF_instance_to_path __P((int ihandle, char *buf, int buflen));
int OF_package_to_path __P((int phandle, char *buf, int buflen));
int OF_call_method_1 __P((char *method, int ihandle, int nargs, ...));
int OF_call_method __P((char *method, int ihandle, int nargs, int nreturns, ...));
int OF_open __P((char *dname));
void OF_close __P((int handle));
int OF_read __P((int handle, void *addr, int len));
int OF_write __P((int handle, void *addr, int len));
int OF_seek __P((int handle, u_quad_t pos));
void OF_boot __P((char *bootspec)) __attribute__((__noreturn__));
void OF_enter __P((void));
void OF_exit __P((void)) __attribute__((__noreturn__));
void (*OF_set_callback __P((void (*newfunc)(void *)))) ();

/*
 * Some generic routines for OpenFirmware handling.
 */
int ofnmmatch __P((char *cp1, char *cp2));

/*
 * Generic OpenFirmware probe argument.
 * This is how all probe structures must start
 * in order to support generic OpenFirmware device drivers.
 */
struct ofprobe {
	int phandle;
	/*
	 * Special unit field for disk devices.
	 * This is a KLUDGE to work around the fact that OpenFirmware
	 * doesn't probe the scsi bus completely.
	 * YES, I THINK THIS IS A BUG IN THE OPENFIRMWARE DEFINITION!!!	XXX
	 * See also ofdisk.c.
	 */
	int unit;
};

/*
 * The softc structure for devices we might be booted from (i.e. we might
 * want to set root/swap to) needs to start with these fields:		XXX
 */
struct ofb_softc {
	struct device sc_dev;
	int sc_phandle;
	int sc_unit;		/* Might be missing for non-disk devices */
};
