/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	OpenBSD
 */
/*	$NetBSD: eisadevs,v 1.1 1996/02/26 23:46:22 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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

struct eisa_knowndev eisa_knowndevs[] = {
	{
	    0,
	    "ADP0000",
	    EISA_PRODUCT_ADP0000,
	},
	{
	    0,
	    "ADP0001",
	    EISA_PRODUCT_ADP0001,
	},
	{
	    0,
	    "ADP0002",
	    EISA_PRODUCT_ADP0002,
	},
	{
	    0,
	    "ADP0400",
	    EISA_PRODUCT_ADP0400,
	},
	{
	    0,
	    "ADP7770",
	    EISA_PRODUCT_ADP7770,
	},
	{
	    0,
	    "ADP7771",
	    EISA_PRODUCT_ADP7771,
	},
	{
	    0,
	    "ADP7756",
	    EISA_PRODUCT_ADP7756,
	},
	{
	    0,
	    "ADP7757",
	    EISA_PRODUCT_ADP7757,
	},
	{
	    0,
	    "DEC4250",
	    EISA_PRODUCT_DEC4250,
	},
	{
	    0,
	    "TCM5092",
	    EISA_PRODUCT_TCM5092,
	},
	{
	    0,
	    "TCM5093",
	    EISA_PRODUCT_TCM5093,
	},
	{
	    EISA_KNOWNDEV_NOPROD,
	    "ADP",
	    "Adaptec",
	},
	{
	    EISA_KNOWNDEV_NOPROD,
	    "BUS",
	    "BusLogic",
	},
	{
	    EISA_KNOWNDEV_NOPROD,
	    "DEC",
	    "Digital Equipment",
	},
	{
	    EISA_KNOWNDEV_NOPROD,
	    "TCM",
	    "3Com",
	},
	{ 0, NULL, NULL, }
};
