/* @(#)error.h	5.18 93/07/30 16:39:48, Srini, AMD */
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
 **       This header file describes the errors which may be returned
 **       by the monitor.
 **
 **       All of the #define'ed error codes below begin with the leters
 **       "EM" (for "Error Monitor").  This should avoid colisions with
 **       other #define's in the system. 
 **
 *****************************************************************************
 */

#ifndef	_ERROR_H_INCLUDED_
#define	_ERROR_H_INCLUDED_

/* General errors */
#define EMUSAGE     1  /* Bad args / flags               */
#define EMFAIL      2  /* Unrecoverable error            */
#define EMBADADDR   3  /* Illegal address                */
#define EMBADREG    4  /* Illegal register               */
#define EMSYNTAX    5  /* Illegal command syntax         */
#define EMACCESS    6  /* Could not access memory        */
#define EMALLOC     7  /* Could not allocate memory      */
#define EMTARGET    8  /* Unknown target type            */
#define EMHINIT     9  /* Could not initialize host      */
#define EMCOMM     10  /* Could not open communication channel */

/* Message errors */
#define EMBADMSG   11  /* Unknown message type           */
#define EMMSG2BIG  12  /* Message to large for buffer    */

#define EMRESET    13  /* Could not RESET target         */
#define EMCONFIG   14  /* Could not get target CONFIG    */
#define EMSTATUS   15  /* Could not get target STATUS    */
#define EMREAD     16  /* Could not READ target memory   */
#define EMWRITE    17  /* Could not WRITE target memory  */
#define EMBKPTSET  18  /* Could not set breakpoint       */
#define EMBKPTRM   19  /* Could not remove breakpoint    */
#define EMBKPTSTAT 20  /* Could not get breakpoint status */
#define EMBKPTNONE 21  /* All breakpoints in use         */
#define EMBKPTUSED 22  /* Breakpoints already in use     */
#define EMCOPY     23  /* Could not COPY target memory   */
#define EMFILL     24  /* Could not FILL target memory   */
#define EMINIT     25  /* Could not initialize target memory */
#define EMGO       26  /* Could not start execution      */
#define EMSTEP     27  /* Could not single step          */
#define EMBREAK    28  /* Could not BREAK                */
#define EMHIF      29  /* Could not perform HIF service  */
#define EMCHANNEL0 30  /* Could not read CHANNEL0        */
#define EMCHANNEL1 31  /* Could not write CHANNEL1       */

/* COFF file loader errors */
#define EMOPEN     32  /* Could not open COFF file       */
#define EMHDR      33  /* Could not read COFF header     */
#define EMMAGIC    34  /* Bad magic number               */
#define EMAOUT     35  /* Could not read COFF a.out header */
#define EMSCNHDR   36  /* Could not read COFF section header */
#define EMSCN      37  /* Could not read COFF section    */
#define EMCLOSE    38  /* Could not close COFF file      */

/* Log file errors */
#define EMLOGOPEN  39  /* Could not open log file        */
#define EMLOGREAD  40  /* Could not read log file        */
#define EMLOGWRITE 41  /* Could not write to log file    */
#define EMLOGCLOSE 42  /* Could not close log file       */

/* Command file errors */
#define EMCMDOPEN  43  /* Could not open command file    */
#define EMCMDREAD  44  /* Could not read command file    */
#define EMCMDWRITE 45  /* Could not write to command file */
#define EMCMDCLOSE 46  /* Could not close comand file    */

#define EMTIMEOUT  47  /* Host timed out waiting for a message */
#define EMCOMMTYPE 48  /* A '-t' flag must be specified  */
#define EMCOMMERR  49  /* Communication error            */
#define EMBAUD     50  /* Invalid baud rate specified    */

#define	EMTIPINIT  51  /* Failed TIP init */
#define	EMIOSETF   52  /* I/O set up failure */
#define EMIORESETF 53  /* I/O reset failure */
#define EMLOADF    54  /* Loading COFF file failed */
#define	EMNOFILE   55	/* No program to run. */
#define	EMECHOPEN  56  	/* Could not open echo file */
#define	EMCTRLC	   57  /* Ctrl-C encountered */
#define	EMNOSUCHCMD  	58	/* Unrecognized command */
#define	EMNOPROCESS	59   /* create process */
#define	EMNOTCOMP	60	/* Not compatible */
#define	EMFATAL		61   /* UDIWait failed */
#define	EMNOINITP	62	/* No initialize process */
#define	EMDOSERR	63	/* DOS err on escape */
#define	EMSYSERR	64	/* system err on escape */
#define	EMINVECHOFILE	65	/* invalid echo file */
#define	EMCMDFILENEST	66	/* command file nesting */

extern	char *error_msg[];

#endif /* _ERROR_H_INCLUDED_ */
