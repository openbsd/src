/*	$OpenBSD: repo.h,v 1.1 2005/02/16 15:41:15 jfb Exp $	*/
/*
 * Copyright (c) 2005 Jean-Francois Brousseau <jfb@openbsd.org>
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

#ifndef REPO_H
#define REPO_H

#include <sys/types.h>
#include <sys/queue.h>


#define CVS_MODULE_ISALIAS     0x01

typedef struct cvs_module {
	char  *cm_name;
	int    cm_flags;
	char  *cm_path;		/* subpath for aliases, NULL otherwise */

	TAILQ_ENTRY(cvs_module) cm_link;
} CVSMODULE;



#define CVS_RPENT_UNKNOWN   0
#define CVS_RPENT_DIR       1
#define CVS_RPENT_RCSFILE   2

typedef struct cvs_repoent CVSRPENT;

/*
 * Repository locks
 * ================
 *
 * OpenCVS derives from the standard CVS mechanism in the way it manages locks
 * on the repository.  GNU CVS uses files with 'rfl' and 'wfl' extensions for
 * read and write locks on particular directories.
 * Using the filesystem for locking semantics has one major drawback: a lock
 * can stay even after the process that created it is gone, if it didn't
 * perform the appropriate cleanup.  This stale lock problem has been known
 * to happen with GNU CVS and an intervention from one of the repository
 * administrators is required before anyone else can access parts of the
 * repository.
 * In OpenCVS, a child cvsd needing to access a particular part of the tree
 * must first request a lock on that part of the tree by issuing a
 * CVS_MSG_LOCK message with the appropriate path.  Although the code
 * supports locking at the file level, it should only be applied to the
 * directory level to avoid extra overhead.  Both read and write locks can be
 * obtained, though with different behaviour.  Multiple simultaneous read locks
 * can be obtained on the same entry, but there can only be one active write
 * lock.  In the case where the directory
 * is already locked by another child, a lock wait is added to that entry
 * and the child requesting the lock will get a CVSD_MSG_LOCKPEND reply,
 * meaning that the lock has not been obtained but the child should block
 * until it receives a CVSD_MSG_OK or CVSD_MSG_ERR telling it whether it
 * obtained the lock or not.  When a child is done modifying the locked portion
 * it should release its lock using the CVSD_MSG_UNLOCK request with the path.
 *
 * NOTES:
 *  * The current locking mechanism allows a lock to be obtained on a
 *    subportion of a part that has already been locked by another process.
 *  * A lock on a directory only allows the owner to modify RCS files found
 *    within that directory.  Any modifications on subdirectories require the
 *    process to lock those subdirectories as well.
 */

#define CVS_LOCK_READ    1
#define CVS_LOCK_WRITE   2


struct cvs_lock {
	pid_t     lk_owner;
	int       lk_type;
	CVSRPENT *lk_ent;		/* backpointer to the entry */

	TAILQ_ENTRY(cvs_lock) lk_link;
	TAILQ_ENTRY(cvs_lock) lk_chlink;
};

TAILQ_HEAD(cvs_lklist, cvs_lock);

struct cvs_repoent {
	char      *cr_name;
	int        cr_type;
	CVSRPENT  *cr_parent;

	union {
		TAILQ_HEAD(, cvs_repoent) files;
	} cr_data;

	struct cvs_lock   *cr_wlock;	/* write lock, NULL if none */
	struct cvs_lklist  cr_rlocks;	/* read locks */
	struct cvs_lklist  cr_lkreq;	/* pending lock requests */

	TAILQ_ENTRY(cvs_repoent) cr_link;
};

#define cr_files cr_data.files



#define CVS_REPO_LOCKED    0x01
#define CVS_REPO_READONLY  0x02
#define CVS_REPO_CHKPERM   0x04

TAILQ_HEAD(cvs_modlist, cvs_module);

typedef struct cvs_repo {
	char     *cr_path;
	int       cr_flags;
	CVSRPENT *cr_tree;

	struct cvs_modlist cr_modules;
	TAILQ_ENTRY(cvs_repo) cr_link;
} CVSREPO;




CVSREPO*  cvs_repo_load      (const char *, int);
void      cvs_repo_free      (CVSREPO *);
int       cvs_repo_alias     (CVSREPO *, const char *, const char *);
int       cvs_repo_unalias   (CVSREPO *, const char *);
int       cvs_repo_lockdir   (CVSREPO *, const char *, int, pid_t);
int       cvs_repo_unlockdir (CVSREPO *, const char *, pid_t);
int       cvs_repo_lockent   (CVSRPENT *, int, pid_t);
int       cvs_repo_unlockent (CVSRPENT *, pid_t);
void      cvs_repo_entfree   (CVSRPENT *);
void      cvs_repo_modfree   (CVSMODULE *);

CVSRPENT* cvs_repo_find      (CVSREPO *, const char *);



#endif /* REPO_H */
