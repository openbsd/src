/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

/*
 * $KTH: fs_local.h,v 1.31 2000/10/03 00:07:40 lha Exp $
 */

#define MAXNAME 100
#define MAXSIZE 2048

struct Acl {
    int NumPositiveEntries;
    int NumNegativeEntries;
    struct AclEntry *pos;
    struct AclEntry *neg;
};

struct AclEntry {
    struct AclEntry *next;
    int32_t RightsMask;
    char name[MAXNAME];
};

struct VolumeStatus {
    int32_t   Vid;
    int32_t   ParentId;
    char      Online;
    char      InService;
    char      Blessed;
    char      NeedsSalvage;
    int32_t   Type;
    int32_t   MinQuota;
    int32_t   MaxQuota;
    int32_t   BlocksInUse;
    int32_t   PartBlocksAvail;
    int32_t   PartMaxBlocks;
};

/* Flags for cell status */
#define PRIMARY_CELL(flags)      ((flags) & CELLSTATUS_PRIMARY)
#define SETUID_HONORED(flags)	 ((flags) & CELLSTATUS_SETUID)
#define OBSOLETE_VERSION(flags)  ((flags) & CELLSTATUS_OBSOLETE_VL)

void afs_copyacl(char *fromdir, char *todir);
void afs_listacl(char *path);
void afs_setacl(char *path, char *user, char *rights);
struct Acl *afs_getacl(char *path);
void afs_sysname(char *name);
void afs_listquota(char *path);
void afs_setmaxquota(char *path, int32_t maxquota);
void afs_whereis(char *path);
void afs_lsmount(const char *path);
void afs_rmmount(const char *path);
void afs_examine(char *path);
int afs_setcache(int, int, int, int);
void afs_whichcell (char *path);
void afs_diskfree (char *path);
void afs_quota (char *path);
void afs_getcellstatus (char *cell);
void afs_getfid(char *path);
int afs_listcells (int printservers, int resolve, int suid);
int afs_connect(int32_t type);
int afs_getcrypt (void);
int afs_setcrypt (int n);
void afs_print_sysname (void);
void afs_set_sysname (const char *sys);
void afs_wscell (void);

void skipline(char **curptr);

/* this program needs __progname defined as a macro */
#define __progname "fs"
#define PROGNAME (fs_interactive ? "" : __progname" ")

/* if this is set the program runs in interactive mode */
static int fs_interactive = 0;
