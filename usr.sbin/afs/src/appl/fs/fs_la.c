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

RCSID("$arla: fs_la.c,v 1.2 2003/05/06 09:13:03 lha Exp $");

static void
skipline(char **curptr)
{
  while(**curptr!='\n') (*curptr)++;
  (*curptr)++;
}

struct Acl *
afs_getacl(char *path)
{
    struct Acl *oldacl;
    struct ViceIoctl a_params;
    struct AclEntry *pos=NULL;
    struct AclEntry *neg=NULL;
    char *curptr;
    char tmpname[MAXNAME];
    int tmprights;
    int i;

    oldacl=(struct Acl *) malloc(sizeof(struct Acl));
    if(oldacl == NULL) {
	printf("fs: Out of memory\n");
	return NULL;
    }

    a_params.in_size=0;
    a_params.out_size=MAXSIZE;
    a_params.in=NULL;
    a_params.out=malloc(MAXSIZE);

    if(a_params.out == NULL) {
	printf ("fs: Out of memory\n");
	free (oldacl);
	return NULL;
    }
    
    if(k_pioctl(path,VIOCGETAL,&a_params,1)==-1) {
	fserr(PROGNAME, errno, path);
	free (oldacl);
	free(a_params.out);
	return NULL;
    }

    curptr=a_params.out;

    /* Number of pos/neg entries parsing */
    sscanf(curptr, "%d\n%d\n", &oldacl->NumPositiveEntries,
	   &oldacl->NumNegativeEntries);
    skipline(&curptr);
    skipline(&curptr);
  
    if(oldacl->NumPositiveEntries)
	for(i=0; i<oldacl->NumPositiveEntries; i++) {      
	    sscanf(curptr, "%99s %d", tmpname, &tmprights);
	    skipline(&curptr);
	    if(!i) {
		pos=malloc(sizeof(struct AclEntry));
		oldacl->pos=pos;
	    }
	    else {
		pos->next=malloc(sizeof(struct AclEntry));
		pos=pos->next;
	    }
	    pos->RightsMask=tmprights;
	    strlcpy(pos->name, tmpname, sizeof(pos->name));
	    pos->next=NULL;
	}

    if(oldacl->NumNegativeEntries)
	for(i=0; i<oldacl->NumNegativeEntries; i++) {      
	    sscanf(curptr, "%99s %d", tmpname, &tmprights);
	    skipline(&curptr);
	    if(!i) {
		neg=malloc(sizeof(struct AclEntry));
		oldacl->neg=neg;
	    }
	    else {
		neg->next=malloc(sizeof(struct AclEntry));
		neg=neg->next;
	    }
	    neg->RightsMask=tmprights;
	    strlcpy(neg->name, tmpname, sizeof(neg->name));
	    neg->next=NULL;
	}

    free(a_params.out);
    return oldacl;
}

static void
afs_listacl(char *path)
{
    struct Acl *acl;
    struct AclEntry *position;
    int i;

    acl = afs_getacl(path);
    if (acl == NULL) {
	if (errno == EACCES)
	    return;
	else
	    exit(1);
    }

    printf("Access list for %s is\n", path);
    if(acl->NumPositiveEntries) {
	printf("Normal rights:\n");

	position=acl->pos;
	for(i=0;i<acl->NumPositiveEntries;i++) {
	    printf("  %s ", position->name);
	    if(position->RightsMask&PRSFS_READ)
		printf("r");
	    if(position->RightsMask&PRSFS_LOOKUP)
		printf("l");
	    if(position->RightsMask&PRSFS_INSERT)
		printf("i");
	    if(position->RightsMask&PRSFS_DELETE)
		printf("d");
	    if(position->RightsMask&PRSFS_WRITE)
		printf("w");
	    if(position->RightsMask&PRSFS_LOCK)
		printf("k");
	    if(position->RightsMask&PRSFS_ADMINISTER)
		printf("a");
	    printf("\n");
	    position=position->next;
	}
    }
    if(acl->NumNegativeEntries) {
	printf("Negative rights:\n");

	position=acl->neg;
	for(i=0;i<acl->NumNegativeEntries;i++) {
	    printf("  %s ", position->name);
	    if(position->RightsMask&PRSFS_READ)
		printf("r");
	    if(position->RightsMask&PRSFS_LOOKUP)
		printf("l");
	    if(position->RightsMask&PRSFS_INSERT)
		printf("i");
	    if(position->RightsMask&PRSFS_DELETE)
		printf("d");
	    if(position->RightsMask&PRSFS_WRITE)
		printf("w");
	    if(position->RightsMask&PRSFS_LOCK)
		printf("k");
	    if(position->RightsMask&PRSFS_ADMINISTER)
		printf("a");
	    printf("\n");
	    position=position->next;
	}
    }
}

int
listacl_cmd (int argc, char **argv)
{
    unsigned int i;

    argc--;
    argv++;

    if(!argc)
      afs_listacl(".");
    else
      for(i=0;i<argc;i++) {
	if(i)
	  printf("\n");
	afs_listacl(argv[i]);
      }

    return 0;
}
