/*	$OpenBSD: version.c,v 1.3 2004/07/29 18:23:25 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sysexits.h>

#include "cvs.h"



int
cvs_version(int argc, char **argv)
{
	if (argc > 1)
		return (EX_USAGE);

	cvs_root = cvsroot_get(".");

	if ((cvs_root) && (cvs_root->cr_method != CVS_METHOD_LOCAL))
		printf("Client: ");

	printf("%s\n", CVS_VERSION);


	if ((cvs_root) && (cvs_root->cr_method != CVS_METHOD_LOCAL))
		if (cvs_client_connect(cvs_root) < 0)
			return (1);

	if ((cvs_root) && (cvs_root->cr_method != CVS_METHOD_LOCAL)) {
		printf("Server: ");
		cvs_client_sendreq(CVS_REQ_VERSION, NULL, 1);
		cvs_client_disconnect(cvs_root);
	}

	return (0);
}
