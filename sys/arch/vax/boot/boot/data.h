/*	$OpenBSD: data.h,v 1.2 2002/06/11 09:36:23 hugh Exp $ */
/*	$NetBSD: data.h,v 1.3 2001/07/26 15:05:09 wiz Exp $ */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by
 * Bertram Barth.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

 /* All bugs are subject to removal without further notice */
		


/*
 * rpb->iovec gives pointer to this structure.
 *
 * bqo->unit_init() is used to initialize the controller,
 * bqo->qio() is used to read from boot-device
 */

struct bqo {
	long  qio;            /*  4  QIO entry  */
	long  map;            /*  4  Mapping entry  */
	long  select;         /*  4  Selection entry  */
	long  drivrname;      /*  4  Offset to driver name  */
	short version;        /*  2  Version number of VMB  */
	short vercheck;       /*  2  Check field  */
	/* offset: 20 */
	long  reselect;       /*  4  Reselection entry  */
	long  move;           /*  4  Move driver entry  */
	long  unit_init;      /*  4  Unit initialization entry  */
	long  auxdrname;      /*  4  Offset to auxiliary driver name  */
	long  umr_dis;        /*  4  UNIBUS Map Registers to disable  */
	/* offset: 40 */
	long  ucode;          /*  4  Absolute address of booting microcode  */
	long  unit_disc;      /*  4  Unit disconnecting entry */
	long  devname;        /*  4  Offset to boot device name */
	long  umr_tmpl;       /*  4  UNIBUS map register template */
	/* offset: 60 */
	/*
	 * the rest is unknown / unnecessary ...
	 */
	long  xxx[6];		/* 24 --	total: 84 bytes */
};
      
extern struct bqo *bqo;
