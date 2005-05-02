/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "fs_local.h"

RCSID("$arla: fs_copyacl.c,v 1.2 2003/04/08 00:01:17 lha Exp $");

static void
afs_copyacl(char *fromdir, char *todir)
{
    struct Acl *acl;
    struct AclEntry *position;
    struct ViceIoctl a_params;
    int i, l, len;
    char acltext[MAXSIZE];

    if((acl=afs_getacl(fromdir))==NULL)
	exit(1);

    l = snprintf(acltext, sizeof(acltext), "%d\n%d\n", 
		 acl->NumPositiveEntries,
		 acl->NumNegativeEntries);
    len = l;
    if (len == -1 || len >= sizeof(acltext)) {
	fserr(PROGNAME, ERANGE, todir);
	return;
    }
    position=acl->pos;
    for(i=0; i<acl->NumPositiveEntries; i++) {
	l = snprintf(acltext + len, sizeof(acltext) - len, 
		     "%s %d\n", position->name, position->RightsMask);
	len += l;
	if (l == -1 || len >= sizeof(acltext)) {
	    fserr(PROGNAME, ERANGE, todir);
	    return;
	}
	position=position->next;
    }
    position=acl->neg;
    for(i=0; i<acl->NumNegativeEntries; i++) {
	l = snprintf(acltext + len, sizeof(acltext) - len, 
		     "%s %d\n", position->name, position->RightsMask);
	len += l;
	if (l == -1 || len >= sizeof(acltext)) {
	    fserr(PROGNAME, ERANGE, todir);
	    return;
	}
	position=position->next;
    }

    a_params.in_size=len;
    a_params.out_size=0;
    a_params.in=acltext;
    a_params.out=0;

    if(k_pioctl(todir,VIOCSETAL,&a_params,1)==-1) {
	fserr(PROGNAME, errno, todir);
	return;
    }
}

int
copyacl_cmd (int argc, char **argv)
{
    argc--;
    argv++;
    
    if (argc != 2) {
	printf ("fs: copyacl: Too many or too few arguments\n");
	return 0;
    }

    afs_copyacl (argv[0], argv[1]);

    return 0;
}
