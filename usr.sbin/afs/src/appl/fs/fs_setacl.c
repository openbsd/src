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

RCSID("$arla: fs_setacl.c,v 1.1 2001/09/24 23:50:20 mattiasa Exp $");

static void
afs_setacl(char *path, char *user, char *rights)
{
    struct Acl *acl;
    struct AclEntry *position;
    struct ViceIoctl a_params;
    int i;
    int newrights=0;
    int foundit=0;
    char *ptr;
    char acltext[MAXSIZE];
    char tmpstr[MAXSIZE];

    if((acl=afs_getacl(path))==NULL)
	exit(1);

    if(!strcmp(rights,"read"))
	newrights=PRSFS_READ | PRSFS_LOOKUP;
    else if(!strcmp(rights,"write"))
	newrights=PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
	    PRSFS_DELETE | PRSFS_WRITE | PRSFS_LOCK;
    else if(!strcmp(rights,"mail"))
	newrights=PRSFS_INSERT | PRSFS_LOCK | PRSFS_LOOKUP;
    else if(!strcmp(rights,"all"))
	newrights=PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
	    PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER;
    else {
	ptr=rights;
	while(*ptr!=0) {
	    if(*ptr=='r')
		newrights|=PRSFS_READ;
	    if(*ptr=='l')
		newrights|=PRSFS_LOOKUP;
	    if(*ptr=='i')
		newrights|=PRSFS_INSERT;
	    if(*ptr=='d')
		newrights|=PRSFS_DELETE;
	    if(*ptr=='w')
		newrights|=PRSFS_WRITE;
	    if(*ptr=='k')
		newrights|=PRSFS_LOCK;
	    if(*ptr=='a')
		newrights|=PRSFS_ADMINISTER;
	    ptr++;
	}
    }

    position=acl->pos;
    for(i=0; i<acl->NumPositiveEntries; i++) {
	if(!strncmp(user, position->name, 100)) {
	    position->RightsMask=newrights;
	    foundit=1;
	}
	if(position->next)
	    position=position->next;
    }

    if(!foundit) {
	if (position) {
	    position->next=malloc(sizeof(struct AclEntry));
	    position=position->next;
	} else {
	    acl->pos = malloc(sizeof(struct AclEntry));
	    position = acl->pos;
	}
	if(position==NULL) {
	    printf("fs: Out of memory\n");
	    exit(1);
	}
	acl->NumPositiveEntries++;

	position->next=NULL;
	strlcpy(position->name, user, sizeof(position->name));
	position->RightsMask=newrights;
    }

    acltext[0] = 0;
    for(position=acl->pos; 
	position && acl->NumPositiveEntries; 
	position = position->next) {
	if (position->RightsMask) {
	    snprintf(tmpstr, sizeof(tmpstr), "%s %d\n",
		     position->name, position->RightsMask);
	    strlcat(acltext, tmpstr, sizeof(acltext));
	} else
	    acl->NumPositiveEntries--;
    }
    for(position=acl->neg; 
	position && acl->NumNegativeEntries;
	position = position->next) {
	if (position->RightsMask) {
	    snprintf(tmpstr, sizeof(tmpstr), "%s %d\n",
		     position->name, position->RightsMask);
	    strlcat(acltext, tmpstr, sizeof(acltext));
	} else
	    acl->NumNegativeEntries--;
    }
    strlcpy (tmpstr, acltext, sizeof(tmpstr));
    snprintf(acltext, sizeof(acltext), "%d\n%d\n%s",
	     acl->NumPositiveEntries, acl->NumNegativeEntries, tmpstr);

    a_params.in_size=strlen(acltext);
    a_params.out_size=0;
    a_params.in=acltext;
    a_params.out=0;


    if(k_pioctl(path,VIOCSETAL,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	return;
    }

    /* XXX  free(oldacl);   and its contents */
}

int
setacl_cmd (int argc, char **argv)
{
    argc--;
    argv++;

    if (argc != 3) {
	printf ("fs: setacl: Too many or too few arguments\n");
	return 0;
    }

    afs_setacl (argv[0], argv[1], argv[2]);

    return 0;
}
