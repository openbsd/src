/*
 *	Transparent Cryptographic File System (TCFS) for NetBSD 
 *	Author and mantainer: 	Luigi Catuogno [luicat@tcfs.unisa.it]
 *	
 *	references:		http://tcfs.dia.unisa.it
 *				tcfs-bsd@tcfs.unisa.it
 */

/*
 *	Base utility set v0.1
 */

#ifndef _TCFSPWDB_H_
#define _TCFSPWDB_H_

#include <sys/param.h>
#include <unistd.h>
#include <limits.h>
#include "tcfsdefines.h"

#define UserLen 	LOGIN_NAME_MAX
#define PassLen 	UUKEYSIZE
#define MaxLineLen 	100
#define MaxUserLen  	LOGIN_NAME_MAX
#define NumOfField   2

typedef struct tcfspwdb_r
{
	char user[LOGIN_NAME_MAX];
	char upw[UUKEYSIZE + 1];
} tcfspwdb;

typedef struct tcfsgpwdb_r
{
	char user[LOGIN_NAME_MAX];
	char gkey[UUGKEYSIZE + 1];
	gid_t gid;
	int n;
	int soglia;
} tcfsgpwdb;

#define U_DEL	0
#define U_NEW	1
#define U_CHG	2
#define U_CKL	3

#define F_USR			0x80
#define F_PWD			0x40
#define F_GID			0x20
#define F_GKEY			0x10
#define F_MEMBERS		0x08
#define F_THRESHOLD	0x04

#define TCFSPWDBSIZ	1024

#define TCFSPWDB	"/etc/tcfs/tcfspwdb"
#define TCFSPWDFILE	"/etc/tcfs/tcfspwdb"
#define TCFSPWDOLD	"/etc/tcfs/tcfspwdb.old"
#define TCFSPWDLOCK	"/etc/tcfs/tcfspwdb.lock"
#define TCFSPWDTMP	"/etc/tcfs/tcfstmp"

#define TCFSGPWDB	"/etc/tcfs/tcfsgpwdb"

#endif /* _TCFSPWDB_H_ */
