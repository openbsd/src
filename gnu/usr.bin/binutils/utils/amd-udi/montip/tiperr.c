/* @(#)tiperr.c	1.26 93/07/30 16:40:38, Srini, AMD */
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
 * This module defines the different TIP Error messages.
 *****************************************************************************
 */
char	*tip_err[] = {
/* 0 */ (char *) 0,
/* TIPNOTIMPLM */ "UDI request not currently implemented by TIP.",
/* TIPPARSECN */  "Could not parse args for Connect.",
/* TIPMSGINIT */  "Could not initialize message system.",
/* TIPCORELOAD */ "Could not load -r rom file.",
/* TIPGOTARGET */ "Error starting target processor.",
/* TIPGETERROR */ "Unknown error number.",
/* TIPSENDCFG */  "Error sending Config message.",
/* TIPRECVCFG */  "Error receiving Config ack.",
/* TIPSENDRST */  "Error sending Reset message.",
/* TIPRECVHLT */  "Error receiving Halt ack.",
/* TIPINITARGS */ "Could not set up Arg Vector for Init.",
/* TIPSENDINIT */ "Error sending Init message.",
/* TIPRECVINIT */ "Error receiving Init ack.",
/* TIPSENDRD */	  "Error sending Read message.",
/* TIPRECVRD */	  "Error receiving Read ack.",
/* TIPSENDWRT */  "Error sending Write message.",
/* TIPRECVWRT */  "Error receiving Write ack.",
/* TIPSENDCPY */  "Error sending Copy message.",
/* TIPRECVCPY */  "Error receiving Copy ack.",
/* TIPSENDGO */	  "Error sending Go message.",
/* TIPRECVGO */	  "Error receiving Go ack.",
/* TIPSENDSTP */  "Error sending Step message.",
/* TIPRECVSTP */  "Error receiving Step ack.",
/* TIPSENDBRK */  "Error sending Break message.",
/* TIPRECVBRK */  "Error receiving Break ack.",
/* TIPSENDSTBP */ "Error sending Set Breakpoint message.",
/* TIPRECVSTBP */ "Error receiving Set Breakpoint ack.",
/* TIPSENDQYBP */ "Error sending Query Breakpoint message.",
/* TIPRECVQYBP */ "Error receiving Query Breakpoint ack.",
/* TIPSENDRMBP */ "Error sending Remove Breakpoint message.",
/* TIPRECVRMBP */ "Error receiving Remove Breakpoint ack.",
/* TIPHIFFAIL */  "Error servicing HIF request.",
/* TIPTIMEOUT */  "Timed out waiting for target.",
/* TIPUNXPMSG */  "Unexpected message received.",
/* TIPINVSPACE */ "Invalid space specified.",
/* TIPINVBID */   "Incorrect Breakpoint ID specified.",
/* TIPNOCORE */   "No core file given.",
/* TIPNOSEND */	  "Could not send a message.",
/* TIPNORECV */   "Could not receive a message.",
/* TIPMSG2BIG */  "Message size exceeds target message buffer size."
};
