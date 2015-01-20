/*	$OpenBSD: config.h,v 1.12 2015/01/20 07:03:21 guenther Exp $	*/

/*
 * Copyright (c) 1993 Michael A. Cooper
 * Copyright (c) 1993 Regents of the University of California.
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

/*
 * $From: config.h,v 1.2 1997/01/05 04:23:35 kim Exp $
 * @(#)config.h
 */

#ifndef __config_h__
#define __config_h__

/*
 * Configuration parameters
 */

/*
 * Check to see if file is on a NFS.  If it is, the file is
 * skipped unless the hostname specified in the Distfile has
 * a trailing "+".  e.g. "foobar+".  This feature is enabled by
 * the -N option.  If your system does not support NFS or you don't
 * want the -N option, undefine this.
 */
#define NFS_CHECK

/*
 * Check to see if file on a Read-Only filesystem.  If it is, no
 * attempt is made to update the file.  This feature is enabled by
 * the -O option.
 */
#define RO_CHECK

/*
 * Default value for the maximum number of clients to update at once.
 * Can be changed with the -M option.
 */
#define MAXCHILDREN 	4

/*
 * Response Time Out interval (in seconds).
 * Should be long enough to allow transfer of large files.
 * The -t option can be used to override this value.
 */
#define RTIMEOUT 	900

/*
 * Syslog levels.  Define these to match the levels you want to log
 * via syslog().  These are defined in <syslog.h>.  If you don't want
 * a particuliar level logged _ever_, undefine it.  What is logged is
 * usually controlled via command line options, so you normally should
 * not need to undefine these.
 */
#define SL_FERROR	LOG_INFO		/* Fatal errors */
#define SL_NERROR	LOG_INFO		/* Normal errors */
#define SL_WARNING	LOG_INFO		/* Warnings */
#define SL_CHANGE	LOG_INFO		/* Things that change */
#define SL_INFO		LOG_INFO		/* General info */
#define SL_NOTICE	LOG_NOTICE		/* General notices */
#define SL_DEBUG	LOG_DEBUG		/* Debugging */

#endif	/* __config_h__ */
