/*	$OpenBSD: common.h,v 1.6 2004/09/20 17:51:07 miod Exp $ */

/*
 * Copyright (c) 1993-95 Mats O Jansson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 *	$OpenBSD: common.h,v 1.6 2004/09/20 17:51:07 miod Exp $
 *
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#define	MAXDL		16		/* maximum number concurrent load */
#define	IFNAME_SIZE	32		/* maximum size if interface name */
#define	BUFSIZE		1600		/* main receive buffer size	*/
#define	HDRSIZ		22		/* room for 803.2 header	*/

#ifndef MOP_FILE_PATH
#define MOP_FILE_PATH	"/tftpboot/mop"
#endif

#define	DEBUG_ONELINE	1
#define	DEBUG_HEADER	2
#define	DEBUG_INFO	3

/*
 * structure per interface
 *
 */

struct if_info {
	int	fd;			/* File Descriptor                 */
	int	trans;			/* Transport type Ethernet/802.3   */
	u_char	eaddr[6];		/* Ethernet addr of this interface */
	char	if_name[IFNAME_SIZE];	/* Interface Name		   */
	int	(*iopen)();		/* Interface Open Routine	   */
	int	(*write)();		/* Interface Write Routine	   */
	void	(*read)();		/* Interface Read Routine          */
	struct if_info *next;		/* Next Interface		   */
};

#define	DL_STATUS_FREE		 0
#define	DL_STATUS_READ_IMGHDR	 1
#define	DL_STATUS_SENT_MLD	 2
#define	DL_STATUS_SENT_PLT	 3

struct dllist {
	u_char	status;			/* Status byte			*/
	struct if_info *ii;		/* interface pointer		*/
	u_char	eaddr[6];		/* targets ethernet address	*/
	int	ldfd;			/* filedescriptor for loadfile	*/
	u_short	dl_bsz;			/* Data Link Buffer Size	*/
	int	timeout;		/* Timeout counter		*/
	u_char	count;			/* Packet Counter		*/
	u_long	loadaddr;		/* Load Address			*/
	u_long	xferaddr;		/* Transfer Address		*/
	u_long	nloadaddr;		/* Next Load Address		*/
	long	lseek;			/* Seek before last read	*/
	int	aout;			/* Is it an a.out file		*/
	u_long	a_text;			/* Size of text segment		*/
	u_long	a_text_fill;		/* Size of text segment fill	*/
	u_long	a_data;			/* Size of data segment		*/
	u_long	a_data_fill;		/* Size of data segment fill	*/
	u_long	a_bss;			/* Size of bss segment		*/
	u_long	a_bss_fill;		/* Size of bss segment fill	*/
	long	a_lseek;		/* Keep track of pos in newfile */
};

#endif /* _COMMON_H_ */
