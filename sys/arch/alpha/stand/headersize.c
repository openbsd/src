/*	$NetBSD: headersize.c,v 1.1 1995/11/23 02:38:59 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/types.h>
#include <sys/exec.h>
#include <machine/coff.h>

#define	HDR_BUFSIZE	512

main()
{
	char buf[HDR_BUFSIZE];

	if (read(0, &buf, HDR_BUFSIZE) < HDR_BUFSIZE) {
		perror("read");
		exit(1);
	}

	printf("%d\n", N_COFFTXTOFF(*((struct filehdr *)buf),
	    *((struct aouthdr *)(buf + sizeof(struct filehdr)))) );
}
