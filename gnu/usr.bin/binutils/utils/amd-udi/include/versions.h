/* @(#)versions.h	5.36 93/11/03 08:33:43, Srini, AMD */
/******************************************************************************
 * Copyright 1991 Advanced Micro Devices, Inc.
 *
 * This software is the property of Advanced Micro Devices, Inc  (AMD)  which
 * specifically  grants the user the right to modify, use and distribute this
 * software provided this notice is not removed or altered.  All other rights
 * are reserved by AMD.
 *
 * AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
 * USE OF THIS SOFTWARE.
 *
 * So that all may benefit from your experience, please report  any  problems
 * or  suggestions about this software to the 29K Technical Support Center at
 * 800-29-29-AMD (800-292-9263) in the USA, or 0800-89-1131  in  the  UK,  or
 * 0031-11-1129 in Japan, toll free.  The direct dial number is 512-462-4118.
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 * 800-292-9263
 *****************************************************************************
 *      Engineer: Srini Subramanian.
 *****************************************************************************
 *****************************************************************************
 */

#ifndef _VERSIONS_H_INCLUDED_
#define	_VERSIONS_H_INCLUDED_

/*   This is the version and date information for the host (DFE) code
	It is included in dfe/main.c   */
/*   This is the version and date information for the host (TIP) code
	It is included in tip/udi2mtip.c   */

#define	MONDFERev	3
#define	MONDFESubRev	3
#define	MONDFESubSubRev	2
#define	MONDFEUDIVers	0x120
#define HOST_VERSION              "3.3-2"
#define HOST_DATE              "03-Nov-93"

#define	MONTIPRev	3
#define	MONTIPSubRev	3
#define	MONTIPSubSubRev	2
#define	MONTIPUDIVers	0x120
#define	TIPVERSION	"3.3-2"
#define	TIPDATE		"03-Nov-93"

#define	LastChange	"hif rtn"

#endif /* _VERSIONS_H_INCLUDED_ */
