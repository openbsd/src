/*	$OpenBSD: klog.h,v 1.3 1998/02/18 11:53:35 art Exp $	*/
/* $KTH: klog.h,v 1.5 1997/05/11 11:05:28 assar Exp $ */

/*
 * This software may now be redistributed outside the US.
 */

/*-
 * Copyright (C) 1989 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America is assumed
 * to require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

/*
 * This file defines the types of log messages logged by klog.  Each
 * type of message may be selectively turned on or off. 
 */

#ifndef KLOG_DEFS
#define KLOG_DEFS

#ifndef KRBLOG
#define KRBLOG 		"/var/log/kerberos.log"  /* master server  */
#endif
#ifndef KRBSLAVELOG
#define KRBSLAVELOG	"/var/log/kerberos_slave.log"  /* slave server  */
#endif
#define	NLOGTYPE	100	/* Maximum number of log msg types  */

#define L_NET_ERR	  1	/* Error in network code	    */
#define L_NET_INFO	  2	/* Info on network activity	    */
#define L_KRB_PERR	  3	/* Kerberos protocol errors	    */
#define L_KRB_PINFO	  4	/* Kerberos protocol info	    */
#define L_INI_REQ	  5	/* Request for initial ticket	    */
#define L_NTGT_INTK       6	/* Initial request not for TGT	    */
#define L_DEATH_REQ       7	/* Request for server death	    */
#define L_TKT_REQ	  8	/* All ticket requests using a tgt  */
#define L_ERR_SEXP	  9	/* Service expired		    */
#define L_ERR_MKV	 10	/* Master key version incorrect     */
#define L_ERR_NKY	 11	/* User's key is null		    */
#define L_ERR_NUN	 12	/* Principal not unique		    */
#define L_ERR_UNK	 13	/* Principal Unknown		    */
#define L_ALL_REQ	 14	/* All requests			    */
#define L_APPL_REQ	 15	/* Application requests (using tgt) */
#define L_KRB_PWARN      16	/* Protocol warning messages	    */

char * klog __P((int type, const char *format, ...))
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
;

#endif /* KLOG_DEFS */
