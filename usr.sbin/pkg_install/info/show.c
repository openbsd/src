/*	$OpenBSD: show.c,v 1.4 1998/10/13 23:09:51 marc Exp $	*/

#ifndef lint
static const char *rcsid = "$OpenBSD: show.c,v 1.4 1998/10/13 23:09:51 marc Exp $";
#endif

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 23 Aug 1993
 *
 * Various display routines for the info module.
 *
 */

#include <err.h>

#include "lib.h"
#include "info.h"

/* structure to define entries for the "show table" */
typedef struct show_t {
	pl_ent_t	sh_type;	/* type of entry */
	char		*sh_quiet;	/* message when quiet */
	char		*sh_verbose;	/* message when verbose */
} show_t;

/* the entries in this table must be ordered the same as pl_ent_t constants */
static show_t	showv[] = {
	{	PLIST_FILE,	"%s",		"File: %s" },
	{	PLIST_CWD,	"@cwd: %s",	"\tCWD to: %s" },
	{	PLIST_CMD,	"@exec %s",	"\tEXEC '%s'" },
	{	PLIST_CHMOD,	"@chmod %s",	"\tCHMOD to %s" },
	{	PLIST_CHOWN,	"@chown %s",	"\tCHOWN to %s" },
	{	PLIST_CHGRP,	"@chgrp %s",	"\tCHGRP to %s" },
	{	PLIST_COMMENT,	"@comment %s",	"\tComment: %s" },
	{	PLIST_IGNORE,	NULL,	NULL },
	{	PLIST_NAME,	"@name %s",	"\tPackage name: %s" },
	{	PLIST_UNEXEC,	"@unexec %s",	"\tUNEXEC '%s'" },
	{	PLIST_SRC,	"@srcdir: %s",	"\tSRCDIR to: %s" },
	{	PLIST_DISPLAY,	"@display %s",	"\tInstall message file: %s" },
	{	PLIST_PKGDEP,	"@pkgdep %s",	"\tPackage depends on: %s" },
	{	PLIST_MTREE,	"@mtree %s",	"\tPackage mtree file: %s" },
	{	PLIST_DIR_RM,	"@dirrm %s",	"\tDeinstall directory remove: %s" },
	{	PLIST_IGNORE_INST,	"@ignore_inst ??? doesn't belong here",
		"\tIgnore next file installation directive (doesn't belong)" },
	{	PLIST_OPTION,	"@option %s",	"\tPackage has option: %s" },
	{	PLIST_PKGCFL,	"@pkgcfl %s",	"\tPackage conflicts with: %s" },
	{	-1,		NULL,		 NULL }
};

void
show_file(char *title, char *fname)
{
	FILE *fp;
	char line[1024];
	int n;

	if (!Quiet) {
		printf("%s%s", InfoPrefix, title);
	}
	if ((fp = fopen(fname, "r")) == (FILE *) NULL) {
		printf("ERROR: show_file: Can't open '%s' for reading!\n", fname);
	} else {
		while ((n = fread(line, 1, 1024, fp)) != 0) {
			fwrite(line, 1, n, stdout);
		}
		(void) fclose(fp);
	}
	printf("\n");	/* just in case */
}

void
show_index(char *title, char *fname)
{
	FILE *fp;
	char line[MAXINDEXSIZE+2];

	if (!Quiet) {
		printf("%s%s", InfoPrefix, title);
	}
	if ((fp = fopen(fname, "r")) == (FILE *) NULL) {
		warnx("show_file: can't open '%s' for reading", fname);
		return;
	}
	if (fgets(line, MAXINDEXSIZE+1, fp)) {
		if (line[MAXINDEXSIZE-1] != '\n') {
			line[MAXINDEXSIZE] = '\n';
		}
		line[MAXINDEXSIZE+1] = 0;
		(void) fputs(line, stdout);
	}
	(void) fclose(fp);
}

/* Show a packing list item type.  If type is PLIST_SHOW_ALL, show all */
void
show_plist(char *title, package_t *plist, pl_ent_t type)
{
    plist_t *p;
    Boolean ign;

    if (!Quiet) {
	printf("%s%s", InfoPrefix, title);
    }
    for (ign = FALSE, p = plist->head; p ; p = p->next) {
	if (p->type == type || type == PLIST_SHOW_ALL) {
		switch(p->type) {
		case PLIST_FILE:
			printf(Quiet ? showv[p->type].sh_quiet : showv[p->type].sh_verbose, p->name);
			if (ign) {
				if (!Quiet) {
					printf(" (ignored)");
				}
				ign = FALSE;
			}
			break;
		case PLIST_CHMOD:
		case PLIST_CHOWN:
		case PLIST_CHGRP:
			printf(Quiet ? showv[p->type].sh_quiet : showv[p->type].sh_verbose,
				p->name ? p->name : "(clear default)");
			break;
		case PLIST_IGNORE:
			ign = TRUE;
			break;
		case PLIST_IGNORE_INST:
			printf(Quiet ? showv[p->type].sh_quiet : showv[p->type].sh_verbose, p->name);
			ign = TRUE;
			break;
		case PLIST_CWD:
		case PLIST_CMD:
		case PLIST_SRC:
		case PLIST_UNEXEC:
		case PLIST_COMMENT:
		case PLIST_NAME:
		case PLIST_DISPLAY:
		case PLIST_PKGDEP:
		case PLIST_MTREE:
		case PLIST_DIR_RM:
		case PLIST_OPTION:
		case PLIST_PKGCFL:
			printf(Quiet ? showv[p->type].sh_quiet : showv[p->type].sh_verbose, p->name);
			break;
		default:
			warnx("unknown command type %d (%s)", p->type, p->name);
		}
		(void) fputc('\n', stdout);
	}
    }
}

/* Show all files in the packing list (except ignored ones) */
void
show_files(char *title, package_t *plist)
{
	plist_t	*p;
	Boolean	ign;
	char	*dir = ".";

	if (!Quiet) {
		printf("%s%s", InfoPrefix, title);
	}
	for (ign = FALSE, p = plist->head; p ; p = p->next) {
		switch(p->type) {
		case PLIST_FILE:
			if (!ign) {
				printf("%s/%s\n", dir, p->name);
			}
			ign = FALSE;
			break;
		case PLIST_CWD:
			dir = p->name;
			break;
		case PLIST_IGNORE:
			ign = TRUE;
			break;
		default:
			break;
		}
	}
}
