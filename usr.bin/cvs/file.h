/*	$OpenBSD: file.h,v 1.11 2004/12/07 17:10:56 tedu Exp $	*/
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

#ifndef FILE_H
#define FILE_H

#include <sys/param.h>

#include <dirent.h>
#include <search.h>

struct cvs_file;
struct cvs_dir;
struct cvs_entries;


#define CVS_FILE_MAXDEPTH     32
#define CVS_FILE_NBUCKETS     256


#define CF_STAT     0x01  /* obsolete */
#define CF_IGNORE   0x02  /* apply regular ignore rules */
#define CF_RECURSE  0x04  /* recurse on directory operations */
#define CF_SORT     0x08  /* all files are sorted alphabetically */
#define CF_KNOWN    0x10  /* only recurse in directories known to CVS */
#define CF_CREATE   0x20  /* create if file does not exist */
#define CF_MKADMIN  0x40  /* create administrative files if they're missing */
#define CF_NOSYMS   0x80  /* ignore symbolic links */


/*
 * The cvs_file structure is used to represent any file or directory within
 * the CVS tree's hierarchy.  The <cf_path> field is a path relative to the
 * directory in which the cvs command was executed.  The <cf_parent> field
 * points back to the parent node in the directory tree structure (it is
 * NULL if the directory is at the wd of the command).
 *
 * The <cf_cvstat> field gives the file's status with regards to the CVS
 * repository.  The file can be in any one of the CVS_FST_* states.
 * If the file's type is DT_DIR, then the <cf_ddat> pointer will point to
 * a cvs_dir structure containing data specific to the directory (such as
 * the contents of the directory's CVS/Entries, CVS/Root, etc.).
 */
#define CVS_FST_UNKNOWN   0
#define CVS_FST_UPTODATE  1
#define CVS_FST_MODIFIED  2
#define CVS_FST_ADDED     3
#define CVS_FST_REMOVED   4
#define CVS_FST_CONFLICT  5
#define CVS_FST_PATCHED   6


struct cvs_fname {
	char  *cf_name;
	u_int  cf_ref;
	SLIST_ENTRY(cvs_fname) cf_list;
};


TAILQ_HEAD(cvs_flist, cvs_file);


typedef struct cvs_file {
	struct cvs_file  *cf_parent;  /* parent directory (NULL if none) */
	struct cvs_fname *cf_name;
	mode_t            cf_mode;
	time_t            cf_mtime;
	u_int16_t         cf_cvstat;  /* cvs status of the file */
	u_int16_t         cf_type;    /* uses values from dirent.h */
	struct cvs_dir   *cf_ddat;    /* only for directories */

	TAILQ_ENTRY(cvs_file)  cf_list;
} CVSFILE;

#define CVS_FILE_NAME(cf)   (cf->cf_name->cf_name)


#define CVS_DIRF_STATIC    0x01
#define CVS_DIRF_STICKY    0x02


struct cvs_dir {
	struct cvsroot     *cd_root;
	char               *cd_repo;
	struct cvs_entries *cd_ent;
	struct cvs_flist    cd_files;
	u_int16_t           cd_nfiles;
	u_int16_t           cd_flags;
};


#define CVS_DIR_ROOT(f)  (((f)->cf_type == DT_DIR) ? \
	(f)->cf_ddat->cd_root : (((f)->cf_parent == NULL) ? \
	NULL : (f)->cf_parent->cf_ddat->cd_root))

#define CVS_DIR_ENTRIES(f)  (((f)->cf_type == DT_DIR) ? \
	(f)->cf_ddat->cd_ent : (((f)->cf_parent == NULL) ? \
	NULL : (f)->cf_parent->cf_ddat->cd_ent))

#define CVS_DIR_REPO(f)  (((f)->cf_type == DT_DIR) ? \
	(f)->cf_ddat->cd_repo : (((f)->cf_parent == NULL) ? \
	NULL : (f)->cf_parent->cf_ddat->cd_repo))

int      cvs_file_init    (void);
int      cvs_file_ignore  (const char *);
int      cvs_file_chkign  (const char *);
CVSFILE* cvs_file_get     (const char *, int);
CVSFILE* cvs_file_getspec (char **, int, int);
CVSFILE* cvs_file_create  (CVSFILE *, const char *, u_int, mode_t);
CVSFILE* cvs_file_copy    (CVSFILE *);
CVSFILE* cvs_file_find    (CVSFILE *, const char *);
int      cvs_file_attach  (CVSFILE *, CVSFILE *);
char*    cvs_file_getpath (CVSFILE *, char *, size_t);
int      cvs_file_examine (CVSFILE *, int (*)(CVSFILE *, void *), void *);
void     cvs_file_free    (CVSFILE *);


#endif /* FILE_H */
