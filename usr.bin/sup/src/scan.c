/*	$OpenBSD: scan.c,v 1.16 2007/09/11 15:47:17 gilles Exp $	*/

/*
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * scan.c - sup list file scanner
 *
 **********************************************************************
 * HISTORY
 * Revision 1.8  92/08/11  12:04:28  mrt
 * 	Brad's changes: delinted, added forward declarations of static
 * 	functions.Added Copyright.
 * 	[92/07/24            mrt]
 * 
 * 18-Mar-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added host=<hostfile> support to releases file.
 *
 * 11-Mar-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added "rsymlink" recursive symbolic link quoting directive.
 *
 * 28-Jun-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code for "release" support.
 *
 * 26-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Lets see if we'll be able to write the scan file BEFORE
 *	we collect the data for it.  Include sys/file.h and use
 *	new definitions for access check codes.
 *
 * 20-May-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added type casting information for lint.
 *
 * 21-Jan-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added check for newonly upgrade when lasttime is the same as
 *	scantime.  This will save us the trouble of parsing the scanfile
 *	when the client has successfully received everything in the
 *	scanfile already.
 *
 * 16-Jan-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Clear Texec pointers in execT so that Tfree of execT will not
 *	free command trees associated with files in listT.
 *
 * 06-Jan-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code to omit scanned files from list if we want new files
 *	only and they are old.
 *
 * 29-Dec-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Major rewrite for protocol version 4.  Added version numbers to
 *	scan file.  Also added mode of file in addition to flags.
 *	Execute commands are now immediately after file information.
 *
 * 13-Dec-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added comments to list file format.
 *
 * 08-Dec-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code to implement omitany.  Currently doesn't know about
 *	{a,b,c} patterns.
 *
 * 07-Oct-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Created.
 *
 **********************************************************************
 */

#include <libc.h>
#include <c.h>
#include <stdlib.h>
#include <vis.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef HAS_POSIX_DIR
#include <dirent.h>
#else
#include <sys/dir.h>
#endif
#include <sys/file.h>
#include <unistd.h>
#include "supcdefs.h"
#include "supextern.h"

/*************************
 ***    M A C R O S    ***
 *************************/

#define SPECNUMBER 1000
	/* number of filenames produced by a single spec in the list file */

/*******************************************
 ***    D A T A   S T R U C T U R E S    ***
 *******************************************/

typedef enum {			/* release options */
	ONEXT,		OPREFIX,	OLIST,		OSCAN,
	OHOST
} OPTION;

static char *options[] = {
	"next",		"prefix",	"list",		"scan",
	"host",
	0
};

typedef enum {			/* <collection>/list file lines */
	LUPGRADE,	LOMIT,		LBACKUP,	LEXECUTE,
	LINCLUDE,	LNOACCT,	LOMITANY,	LALWAYS,
	LSYMLINK,	LRSYMLINK
} LISTTYPE;

static char *ltname[] = {
	"upgrade",	"omit",		"backup",	"execute",
	"include",	"noaccount",	"omitany",	"always",
	"symlink",	"rsymlink",
	0
};

#define FALWAYS		FUPDATE

/* list file lines */
static TREE *upgT;			/* files to upgrade */
static TREE *flagsT;			/* special flags: BACKUP NOACCT */
static TREE *omitT;			/* recursize file omition list */
static TREE *omanyT;			/* non-recursize file omition list */
static TREE *symT;			/* symbolic links to quote */
static TREE *rsymT;			/* recursive symbolic links to quote */
static TREE *execT;			/* execute command list */

/*************************
 ***    E X T E R N    ***
 *************************/

#ifdef	lint
static char _argbreak;
#else
extern char _argbreak;			/* break character from nxtarg */
#endif

extern TREELIST *listTL;		/* list of trees for scanning */
extern TREE *listT;			/* final list of files in collection */
extern TREE *refuseT;			/* files refused by client */

extern char *collname;			/* collection name */
extern char *basedir;			/* base directory name */
extern char *prefix;			/* collection pathname prefix */
extern time_t lasttime;			/* time of last upgrade */
extern time_t scantime;			/* time of this scan */
extern int trace;			/* trace directories */
extern int newonly;			/* new files only */

#ifdef RCSSTAT
extern char *rcs_branch;
extern int candorcs;
#endif

/*************************************************
 ***   STATIC   R O U T I N E S    ***
 *************************************************/
static void passdelim(char **, int );
static char *parserelease(TREELIST **, char *, char *);
static int scanone(TREE *, void *);
static void makescan(char *, char *);
static void getscan(char *, char *);
static void doscan(char *);
static void readlistfile(char *);
static void expTinsert(char *, TREE **, int, char *);
static int listone(TREE *, void *);
static void listentry(char *, char *, char *, int);
static void listname(char *, struct stat *);
static void listdir(char *, int);
static int omitanyone(TREE *, void *);
static int anyglob(char *, char *);
static int getscanfile(char *);
static void chkscanfile(char *);
static void makescanfile(char *);
static int recordone(TREE *, void *);
static int recordexec(TREE *, void *);


/*************************************************
 ***    L I S T   S C A N   R O U T I N E S    ***
 *************************************************/

static void
passdelim(ptr,delim)		/* skip over delimiter */
	char **ptr, delim;
{

	*ptr = skipover (*ptr, " \t");
	if (_argbreak != delim && **ptr == delim) {
		(*ptr)++;
		*ptr = skipover (*ptr, " \t");
	}
}

static char *
parserelease(tlp, relname, args)
	TREELIST **tlp;
	char *relname, *args;
{
	TREELIST *tl;
	char *arg;
	OPTION option;
	int opno;
	char *nextrel;

	tl = (TREELIST *) malloc(sizeof(TREELIST));
	if ((*tlp = tl) == NULL)
		goaway("Couldn't allocate TREELIST");
	tl->TLnext = NULL;
	tl->TLname = strdup(relname);
	tl->TLprefix = NULL;
	tl->TLlist = NULL;
	tl->TLscan = NULL;
	tl->TLhost = NULL;
	nextrel = NULL;
	args = skipover(args, " \t");
	while (*(arg = nxtarg(&args, " \t="))) {
		for (opno = 0; options[opno] != NULL; opno++)
			if (strcmp(arg, options[opno]) == 0)
				break;
		if (options[opno] == NULL)
			goaway("Invalid release option %s for release %s",
			    arg, relname);
		option = (OPTION) opno;
		switch (option) {
		case ONEXT:
			passdelim(&args, '=');
			arg = nxtarg(&args, " \t");
			nextrel = strdup(arg);
			break;
		case OPREFIX:
			passdelim(&args, '=');
			arg = nxtarg(&args, " \t");
			tl->TLprefix = strdup(arg);
			break;
		case OLIST:
			passdelim(&args,'=');
			arg = nxtarg(&args, " \t");
			tl->TLlist = strdup(arg);
			break;
		case OSCAN:
			passdelim(&args, '=');
			arg = nxtarg(&args, " \t");
			tl->TLscan = strdup(arg);
			break;
		case OHOST:
			passdelim(&args, '=');
			arg = nxtarg (&args, " \t");
			tl->TLhost = strdup(arg);
			break;
		}
	}
	return (nextrel);
}

int
getrelease (release)
	char *release;
{
	TREELIST *tl;
	char buf[STRINGLENGTH];
	char *p,*q;
	int rewound;
	FILE *f;

	if (release == NULL)
		release = strdup(DEFRELEASE);
	listTL = NULL;

	(void) snprintf(buf, sizeof buf, FILERELEASES, collname);
	f = fopen(buf, "r");
	if (f != NULL) {
		rewound = TRUE;
		for (;;) {
			p = fgets(buf, sizeof(buf), f);
			if (p == NULL) {
				if (rewound)
					break;
				rewind(f);
				rewound = TRUE;
				continue;
			}
			p[strcspn(p, "\n")] = '\0';
			if (strchr("#;:", *p))
				continue;
			q = nxtarg(&p, " \t");
			if (strcmp(q, release) != 0)
				continue;
			release = parserelease(&tl, release, p);
			if (tl->TLprefix == NULL)
				tl->TLprefix = prefix;
			else if (chdir(tl->TLprefix) < 0)
				return (FALSE);
			else
				(void) chdir(basedir);
			tl->TLnext = listTL;
			listTL = tl;
			if (release == NULL)
				break;
			rewound = FALSE;
		}
		(void) fclose(f);
	}
	if (release == NULL)
		return (TRUE);
	if (strcmp(release, DEFRELEASE) != 0)
		return (FALSE);
	(void) parserelease(&tl, release, "");
	tl->TLprefix = prefix;
	tl->TLnext = listTL;
	listTL = tl;
	return (TRUE);
}

void
makescanlists()
{
	TREELIST *tl;
	char buf[STRINGLENGTH];
	char *p,*q;
	FILE *f;
	char *saveprefix = prefix;
	int count = 0;

	(void) snprintf(buf, sizeof buf, FILERELEASES, collname);
	f = fopen(buf, "r");
	if (f != NULL) {
		while ((p = fgets(buf, sizeof(buf), f)) != NULL) {
			p[strcspn(p, "\n")] = '\0';
			if (strchr("#;:", *p))
				continue;
			q = nxtarg(&p, " \t");
			(void) parserelease(&tl, q, p);
			if ((prefix = tl->TLprefix) == NULL)
				prefix = saveprefix;
			if (prefix != NULL) {
				if (chdir(prefix) < 0)
					goaway("Can't chdir to %s", prefix);
				(void) chdir(basedir);
			}
			makescan(tl->TLlist, tl->TLscan);
			free(tl);
			count++;
		}
		(void) fclose(f);
	}
	if (count == 0)
		makescan(NULL, NULL);
}

static int
scanone(t, v)
	TREE *t;
	void *v;
{
	TREE *newt;

	if (newonly && (t->Tflags&FNEW) == 0)
		return (SCMOK);
	newt = Tinsert (&listT, t->Tname, FALSE);
	if (newt == NULL)
		return (SCMOK);
	newt->Tmode = t->Tmode;
	newt->Tflags = t->Tflags;
	newt->Tmtime = t->Tmtime;
	return (SCMOK);
}

void
getscanlists()
{
	TREELIST *tl,*stl;

	stl = listTL;
	listTL = NULL;
	while ((tl = stl) != NULL) {
		prefix = tl->TLprefix;
		getscan(tl->TLlist, tl->TLscan);
		tl->TLtree = listT;
		stl = tl->TLnext;
		tl->TLnext = listTL;
		listTL = tl;
	}
	listT = NULL;
	for (tl = listTL; tl != NULL; tl = tl->TLnext)
		(void) Tprocess(tl->TLtree, scanone, NULL);
}

static void
makescan(listfile, scanfile)
	char *listfile, *scanfile;
{
	listT = NULL;
	chkscanfile(scanfile);		/* can we can write a scan file? */
	doscan(listfile);		/* read list file and scan disk */
	makescanfile(scanfile);		/* record names in scan file */
	Tfree(&listT);			/* free file list tree */
}

static void
getscan(listfile, scanfile)
	char *listfile, *scanfile;
{
	listT = NULL;
	if (!getscanfile(scanfile)) {	/* check for pre-scanned file list */
		scantime = time(NULL);
		doscan(listfile);	/* read list file and scan disk */
	}
}

static void
doscan (listfile)
	char *listfile;
{
	char buf[STRINGLENGTH];

	upgT = NULL;
	flagsT = NULL;
	omitT = NULL;
	omanyT = NULL;
	execT = NULL;
	symT = NULL;
	rsymT = NULL;
	if (listfile == NULL)
		listfile = FILELISTDEF;
	(void) snprintf(buf, sizeof buf, FILELIST, collname, listfile);
	readlistfile(buf);		/* get contents of list file */
	(void) Tprocess(upgT, listone, NULL); /* build list of files specified */
	cdprefix(NULL);
	Tfree(&upgT);
	Tfree(&flagsT);
	Tfree(&omitT);
	Tfree(&omanyT);
	Tfree(&execT);
	Tfree(&symT);
	Tfree(&rsymT);
}

static void
readlistfile(fname)
	char *fname;
{
	char buf[STRINGLENGTH+MAXPATHLEN*4+1], *p;
	char *q, *r;
	FILE *f;
	int ltn, n, i, flags;
	TREE **t = NULL;
	LISTTYPE lt;
	char *speclist[SPECNUMBER];

	f = fopen(fname,"r");
	if (f == NULL)
		goaway("Can't read list file %s", fname);
	cdprefix(prefix);
	while ((p = fgets(buf, sizeof(buf), f)) != NULL) {
		p[strcspn(p, "\n")] = '\0';
		if (strchr("#;:", *p))
			continue;
		q = nxtarg (&p, " \t");
		if (*q == '\0')
			continue;
		for (ltn = 0; ltname[ltn] && strcmp(q, ltname[ltn]) != 0; ltn++)
			;
		if (ltname[ltn] == NULL)
			goaway("Invalid list file keyword %s", q);
		lt = (LISTTYPE) ltn;
		flags = 0;
		switch (lt) {
		case LUPGRADE:
			t = &upgT;
			break;
		case LBACKUP:
			t = &flagsT;
			flags = FBACKUP;
			break;
		case LNOACCT:
			t = &flagsT;
			flags = FNOACCT;
			break;
		case LSYMLINK:
			t = &symT;
			break;
		case LRSYMLINK:
			t = &rsymT;
			break;
		case LOMIT:
			t = &omitT;
			break;
		case LOMITANY:
			t = &omanyT;
			break;
		case LALWAYS:
			t = &upgT;
			flags = FALWAYS;
			break;
		case LINCLUDE:
			while (*(q = nxtarg(&p, " \t"))) {
				cdprefix(NULL);
				n = expand(q, speclist, SPECNUMBER);
				for (i = 0; i < n && i < SPECNUMBER; i++) {
					readlistfile(speclist[i]);
					cdprefix(NULL);
					free(speclist[i]);
				}
				cdprefix(prefix);
			}
			continue;
		case LEXECUTE:
			r = p = q = skipover(p, " \t");
			do {
				q = p = skipto(p, " \t(");
				p = skipover(p, " \t");
			} while (*p != '(' && *p != '\0');
			if (*p++ == '(') {
				*q = '\0';
				do {
					q = nxtarg(&p, " \t)");
					if (*q == 0)
						_argbreak = ')';
					else
						expTinsert(q, &execT, 0, r);
				} while (_argbreak != ')');
				continue;
			}
			/* fall through */
		default:
			goaway("Error in handling list file keyword %d", ltn);
		}
		while (*(q = nxtarg(&p, " \t"))) {
			if (lt == LOMITANY)
				(void) Tinsert(t, q, FALSE);
			else
				expTinsert(q, t, flags, NULL);
		}
	}
	(void) fclose(f);
}

static void
expTinsert(p, t, flags, exec)
	char *p;
	TREE **t;
	int flags;
	char *exec;
{
	int n, i;
	TREE *newt;
	char *speclist[SPECNUMBER];
	char buf[STRINGLENGTH];

	n = expand(p, speclist, SPECNUMBER);
	for (i = 0; i < n && i < SPECNUMBER; i++) {
		newt = Tinsert(t, speclist[i], TRUE);
		newt->Tflags |= flags;
		if (exec) {
			(void) snprintf(buf, sizeof buf, exec, speclist[i]);
			(void) Tinsert(&newt->Texec, buf, FALSE);
		}
		free(speclist[i]);
	}
}

static int
listone (t, v)		/* expand and add one name from upgrade list */
	TREE *t;
	void *v;
{

	listentry(t->Tname, t->Tname, NULL, (t->Tflags&FALWAYS) != 0);
	return (SCMOK);
}

static void
listentry(name, fullname, updir, always)
	char *name, *fullname, *updir;
	int always;
{
	struct stat statbuf;
	int link = 0;

	if (Tlookup (refuseT, fullname))
		return;
	if (!always) {
		if (Tsearch (omitT, fullname))
			return;
		if (Tprocess (omanyT, omitanyone, fullname) != SCMOK)
			return;
	}
	if (lstat(name, &statbuf) < 0)
		return;
	if (S_ISLNK(statbuf.st_mode)) {
		if (Tsearch(symT, fullname)) {
			listname(fullname, &statbuf);
			return;
		}
		if (Tlookup(rsymT, fullname)) {
			listname(fullname, &statbuf);
			return;
		}
		if (updir)
			link++;
		if (stat(name, &statbuf) < 0)
			return;
	}
	if (S_ISDIR(statbuf.st_mode)) {
		if (access(name, R_OK|X_OK) < 0)
			return;
		if (chdir(name) < 0)
			return;
		listname(fullname, &statbuf);
		if (trace) {
			printf("Scanning directory %s\n", fullname);
			(void) fflush(stdout);
		}
		listdir (fullname, always);
		if (updir == 0 || link) {
			(void) chdir(basedir);
			if (prefix)
				(void) chdir(prefix);
			if (updir && *updir)
				(void) chdir(updir);
		} else
			(void) chdir("..");
		return;
	}
	if (access(name, R_OK) < 0)
		return;
#ifdef RCSSTAT
        if (candorcs) {
                char rcs_release[STRINGLENGTH];
                int status;

                if (rcs_branch != NULL)
#ifdef CVS
                        snprintf(rcs_release, sizeof rcs_release,
				"-r %s", rcs_branch);
#else
                        snprintf(rcs_release, sizeof rcs_release,
				"-r%s", rcs_branch);
#endif
                else
                        rcs_release[0] = '\0';
#ifdef CVS
                snprintf(sys_com, sizeof sys_com,
		    "cvs -d %s -r -l -Q co -p %s %s > %s\n", cvs_root,
		    rcs_release, name, rcs_file);
#else
                status = runp("rcsstat", "rcsstat", "-q", rcs_release, name, 0);
#endif
                if (status != 0)
			return;
        }
#endif
	listname(fullname, &statbuf);
}

static void
listname(name,st)
	char *name;
	struct stat *st;
{
	TREE *t, *ts;
	int new;
	TREELIST *tl;

	new = st->st_ctime > lasttime;
	if (newonly && !new) {
		for (tl = listTL; tl != NULL; tl = tl->TLnext)
			if ((ts = Tsearch(tl->TLtree, name)) != NULL)
				ts->Tflags &= ~FNEW;
		return;
	}
	t = Tinsert(&listT, name, FALSE);
	if (t == NULL)
		return;
	t->Tmode = st->st_mode;
	t->Tctime = st->st_ctime;
	t->Tmtime = st->st_mtime;
	if (new)
		t->Tflags |= FNEW;
	if ((ts = Tsearch (flagsT, name)) != NULL)
		t->Tflags |= ts->Tflags;
	if ((ts = Tsearch (execT, name)) != NULL) {
		t->Texec = ts->Texec;
		ts->Texec = NULL;
	}
}

static void
listdir(name,always)		/* expand directory */
	char *name;
	int always;
{
#ifdef HAS_POSIX_DIR
	struct dirent *dentry;
#else
	struct direct *dentry;
#endif
	DIR *dirp;
	char ename[STRINGLENGTH], newname[STRINGLENGTH], filename[STRINGLENGTH];
	char *p, *newp;
	int i;

	dirp = opendir(".");
	if (dirp == 0)
		return;	/* unreadable: probably protected */

	p = name;		/* punt leading ./ and trailing / */
	newp = newname;
	if (p[0] == '.' && p[1] == '/') {
		p += 2;
		while (*p == '/')
			p++;
	}
	while ((*newp++ = *p++) != '\0')
		;	/* copy string */
	--newp;				/* trailing null */
	while (newp > newname && newp[-1] == '/')
		--newp; /* trailing / */
	*newp = 0;
	if (strcmp(newname, ".") == 0)
		newname[0] = 0;	/* "." ==> "" */

	while ((dentry=readdir(dirp)) != NULL) {
		if (dentry->d_ino == 0 || strcmp(dentry->d_name, ".") == 0 ||
		    strcmp(dentry->d_name, "..") == 0)
			continue;
		for (i=0; i <= MAXNAMLEN && dentry->d_name[i]; i++)
			ename[i] = dentry->d_name[i];
		ename[i] = 0;
		if (*newname)
			(void) snprintf(filename, sizeof filename, "%s/%s",
			    newname, ename);
		else
			(void) strlcpy(filename, ename, sizeof filename);
		listentry(ename, filename, newname, always);
	}
	closedir(dirp);
}

static int
omitanyone(t,fv)
	TREE *t;
	void *fv;
{
	char *filename = fv;

	if (anyglob(t->Tname, filename))
		return (SCMERR);
	return (SCMOK);
}

static int
anyglob(pattern, match)
	char *pattern, *match;
{
	char *p, *m;
	char *pb, *pe;

	p = pattern; 
	m = match;
	while (*m && *p == *m ) { 
		p++; 
		m++; 
	}
	if (*p == '\0' && *m == '\0')
		return (TRUE);
	switch (*p++) {
	case '*':
		for (;;) {
			if (*p == '\0')
				return (TRUE);
			if (*m == '\0')
				return (*p == '\0');
			if (anyglob (p, ++m))
				return (TRUE);
		}
	case '?':
		return (anyglob(p, ++m));
	case '[':
		pb = p;
		while (*(++p) != ']')
			if (*p == '\0')
				return (FALSE);
		pe = p;
		for (p = pb + 1; p != pe; p++) {
			switch (*p) {
			case '-':
				if (p == pb && *m == '-') {
					p = pe + 1;
					return (anyglob(p, ++m));
				}
				if (p == pb)
					continue;
				if ((p + 1) == pe)
					return (FALSE);
				if (*m > *(p - 1) &&
				    *m <= *(p + 1)) {
					p = pe + 1;
					return (anyglob(p, ++m));
				}
				continue;
			default:
				if (*m == *p) {
					p = pe + 1;
					return (anyglob(p, ++m));
				}
			}
		}
		return (FALSE);
	default:
		return (FALSE);
	}
}

/*****************************************
 ***    R E A D   S C A N   F I L E    ***
 *****************************************/

static
int getscanfile(scanfile)
	char *scanfile;
{
	char buf[STRINGLENGTH];
	char fname[MAXPATHLEN];
	struct stat sbuf;
	FILE *f;
	TREE ts;
	char *p, *q;
	TREE *tmp, *t = NULL;
	int notwanted;
	TREELIST *tl;
        
	if (scanfile == NULL)
		scanfile = FILESCANDEF;
	(void) snprintf(buf, sizeof buf, FILESCAN, collname, scanfile);
	if (stat(buf, &sbuf) < 0)
		return (FALSE);
	if ((f = fopen(buf, "r")) == NULL)
		return (FALSE);
	if ((p = fgets (buf, sizeof(buf), f)) == NULL) {
		(void) fclose(f);
		return (FALSE);
	}

	p[strcspn(p, "\n")] = '\0';

	if (*p++ != 'V') {
		(void) fclose(f);
		return (FALSE);
	}
	if (atoi (p) != SCANVERSION) {
		(void) fclose(f);
		return (FALSE);
	}
	scantime = sbuf.st_mtime;	/* upgrade time is time of supscan,
					 * i.e. time of creation of scanfile */
	if (newonly && scantime == lasttime) {
		(void) fclose(f);
		return (TRUE);
	}
	notwanted = FALSE;
	while ((p = fgets(buf, sizeof(buf), f)) != NULL) {
		p[strcspn(p, "\n")] = '\0';
		ts.Tflags = 0;
		if (*p == 'X') {
			if (notwanted)
				continue;
			if (t == NULL)
				goaway("scanfile format inconsistent");
			(void) Tinsert(&t->Texec, ++p, FALSE);
			continue;
		}
		notwanted = FALSE;
		if (*p == 'B') {
			p++;
			ts.Tflags |= FBACKUP;
		}
		if (*p == 'N') {
			p++;
			ts.Tflags |= FNOACCT;
		}
		if ((q = strchr(p, ' ')) == NULL)
			goaway("scanfile format inconsistent");
		*q++ = '\0';
		ts.Tmode = atoo(p);
		p = q;
		if ((q = strchr(p, ' ')) == NULL)
			goaway("scanfile format inconsistent");
		*q++ = '\0';
		ts.Tctime = atoi(p);
		p = q;
		if ((q = strchr(p, ' ')) == NULL)
			goaway("scanfile format inconsistent");
		*q++ = 0;
		ts.Tmtime = atoi(p);
		(void) strunvis(fname, q);
		if (ts.Tctime > lasttime)
			ts.Tflags |= FNEW;
		else if (newonly) {
			for (tl = listTL; tl != NULL; tl = tl->TLnext)
				if ((tmp = Tsearch(tl->TLtree, fname)) != NULL)
					tmp->Tflags &= ~FNEW;
			notwanted = TRUE;
			continue;
		}
		if (Tlookup(refuseT, fname)) {
			notwanted = TRUE;
			continue;
		}
		t = Tinsert(&listT, fname, TRUE);
		t->Tmode = ts.Tmode;
		t->Tflags = ts.Tflags;
		t->Tctime = ts.Tctime;
		t->Tmtime = ts.Tmtime;
	}
	(void) fclose(f);
	return (TRUE);
}

/*******************************************
 ***    W R I T E   S C A N   F I L E    ***
 *******************************************/

static void chkscanfile(scanfile)
char *scanfile;
{
	char tname[STRINGLENGTH], fname[STRINGLENGTH];
	FILE *f;

	if (scanfile == NULL)
		scanfile = FILESCANDEF;
	(void) snprintf(fname, sizeof fname, FILESCAN, collname, scanfile);
	(void) snprintf(tname, sizeof tname, "%s.temp", fname);
	if ((f = fopen(tname, "w")) == NULL)
		goaway("Can't test scan file temp %s for %s", tname, collname);
	else {
		(void) unlink(tname);
		(void) fclose(f);
	}
}

static void makescanfile(scanfile)
	char *scanfile;
{
	char tname[STRINGLENGTH],fname[STRINGLENGTH];
	struct timeval tbuf[2];
	FILE *scanF;			/* output file for scanned file list */

	if (scanfile == NULL)
		scanfile = FILESCANDEF;
	(void) snprintf(fname, sizeof fname, FILESCAN, collname, scanfile);
	(void) snprintf(tname, sizeof tname, "%s.temp", fname);
	scanF = fopen(tname, "w");
	if (scanF == NULL)
		goaway("Can't write scan file temp %s for %s", tname, collname);
	fprintf(scanF, "V%d\n", SCANVERSION);
	(void) Tprocess(listT, recordone, scanF);
	(void) fclose(scanF);
	if (rename(tname, fname) < 0)
		goaway("Can't change %s to %s", tname, fname);
	(void) unlink(tname);
	tbuf[0].tv_sec = time(NULL);
	tbuf[0].tv_usec = 0;
	tbuf[1].tv_sec = scantime;
	tbuf[1].tv_usec = 0;
	(void) utimes(fname, tbuf);
}

static int
recordone(t,v)
	TREE *t;
	void *v;
{
	FILE *scanF = v;
	char fname[MAXPATHLEN*4+1];

	if (t->Tflags&FBACKUP)
		fprintf(scanF, "B");
	if (t->Tflags&FNOACCT)
		fprintf(scanF, "N");
	strnvis(fname, t->Tname, sizeof fname, VIS_WHITE);
	fprintf(scanF, "%o %d %d %s\n", t->Tmode, t->Tctime, t->Tmtime, fname);
	(void) Tprocess(t->Texec, recordexec, scanF);
	return (SCMOK);
}

static int
recordexec(t,v)
	TREE *t;
	void *v;
{
	FILE *scanF = v;
	char fname[MAXPATHLEN*4+1];

	strlcpy(fname, t->Tname, sizeof fname);
	fprintf(scanF, "X%s\n", fname);
	return (SCMOK);
}

void
cdprefix(prefix)
	char *prefix;
{
	static char *curprefix = NULL;

	if (curprefix == NULL) {
		if (prefix == NULL)
			return;
		(void) chdir(prefix);
		curprefix = prefix;
		return;
	}
	if (prefix == NULL) {
		(void) chdir(basedir);
		curprefix = NULL;
		return;
	}
	if (prefix == curprefix)
		return;
	if (strcmp (prefix, curprefix) == 0) {
		curprefix = prefix;
		return;
	}
	(void) chdir(basedir);
	(void) chdir(prefix);
	curprefix = prefix;
}
