/*	$OpenBSD: entries.c,v 1.2 2004/07/14 04:32:42 jfb Exp $	*/
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

#include <sys/param.h>
#include <sys/stat.h>

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "log.h"
#include "cvs.h"


#define CVS_ENTRIES_NFIELDS  5
#define CVS_ENTRIES_DELIM   '/'


/*
 * cvs_ent_open()
 *
 * Open the CVS Entries file for the directory <dir>.
 * Returns a pointer to the CVSENTRIES file structure on success, or NULL
 * on failure.
 */

CVSENTRIES*
cvs_ent_open(const char *dir, int flags)
{
	size_t len;
	char entpath[MAXPATHLEN], ebuf[128];
	FILE *fp;
	struct cvs_ent *ent;
	CVSENTRIES *ep;

	snprintf(entpath, sizeof(entpath), "%s/" CVS_PATH_ENTRIES, dir);
	fp = fopen(entpath, "r");
	if (fp == NULL) {
		cvs_log(LP_ERRNO, "cannot open CVS/Entries for reading",
		    entpath);
		return (NULL);
	}

	ep = (CVSENTRIES *)malloc(sizeof(CVSENTRIES));
	if (ep == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate Entries data");
		(void)fclose(fp);
		return (NULL);
	}
	ep->cef_path = strdup(dir);
	if (ep->cef_path == NULL) {
		cvs_log(LP_ERRNO, "failed to copy Entries path");
		free(ep);
		(void)fclose(fp);
		return (NULL);
	}

	ep->cef_nid = 0;
	ep->cef_entries = NULL;
	ep->cef_nbent = 0;

	while (fgets(ebuf, sizeof(ebuf), fp) != NULL) {
		len = strlen(ebuf);
		if ((len > 0) && (ebuf[len - 1] == '\n'))
			ebuf[--len] = '\0';
		ent = cvs_ent_parse(ebuf);
		if (ent == NULL)
			continue;

		if (cvs_ent_add(ep, ent) < 0) {
			cvs_ent_close(ep);
			ep = NULL;
			break;
		}
	}

	(void)fclose(fp);
	return (ep);
}


/*
 * cvs_ent_close()
 *
 * Close the Entries file <ep>.
 */

void
cvs_ent_close(CVSENTRIES *ep)
{
	free(ep);
}


/*
 * cvs_ent_add()
 *
 * Add the entry <ent>
 */

int
cvs_ent_add(CVSENTRIES *ef, struct cvs_ent *ent)
{
	void *tmp;
	struct cvs_ent *entp;

	if (cvs_ent_get(ef, ent->ce_name) != NULL)
		return (-1);

	entp = cvs_ent_parse(ent->ce_line);
	if (entp == NULL) {
		return (-1);
	}

	tmp = realloc(ef->cef_entries, (ef->cef_nbent + 1) * sizeof(entp));
	if (tmp == NULL) {
		cvs_log(LP_ERRNO, "failed to resize entries buffer");
		return (-1);
	}

	ef->cef_entries = (struct cvs_ent **)tmp;
	ef->cef_entries[ef->cef_nbent++] = entp;

	return (0);
}


/*
 * cvs_ent_get()
 *
 * Get the CVS entry from the Entries file <ef> whose 'name' portion matches
 * <file>.
 * Returns a pointer to the cvs entry structure on success, or NULL on failure.
 */

struct cvs_ent*
cvs_ent_get(CVSENTRIES *ef, const char *file)
{
	u_int i;

	for (i = 0; i < ef->cef_nbent; i++) {
		if (strcmp(ef->cef_entries[i]->ce_name, file) == 0)
			return ef->cef_entries[i]; 
	}

	return (NULL);
}


/*
 * cvs_ent_next()
 *
 * Returns a pointer to the cvs entry structure on success, or NULL on failure.
 */

struct cvs_ent*
cvs_ent_next(CVSENTRIES *ef)
{
	if (ef->cef_nid >= ef->cef_nbent)
		return (NULL);

	return (ef->cef_entries[ef->cef_nid++]);
}


/*
 * cvs_ent_parse()
 *
 * Parse a single line from a CVS/Entries file and return a cvs_entry structure
 * containing all the parsed information.
 */

struct cvs_ent*
cvs_ent_parse(const char *entry)
{
	int i;
	char *fields[CVS_ENTRIES_NFIELDS], *sp, *dp;
	struct cvs_ent *entp;

	entp = (struct cvs_ent *)malloc(sizeof(*entp));
	if (entp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate CVS entry");
		return (NULL);
	}

	entp->ce_rev = rcsnum_alloc();
	if (entp->ce_rev == NULL) {
		free(entp);
		return (NULL);
	}

	entp->ce_line = strdup(entry);
	if (entp->ce_line == NULL) {
		free(entp);
		return (NULL);
	}

	entp->ce_buf = strdup(entry);
	if (entp->ce_buf == NULL) {
		free(entp->ce_line);
		free(entp);
		return (NULL);
	}
	sp = entp->ce_buf;

	if (*sp == CVS_ENTRIES_DELIM)
		entp->ce_type = CVS_ENT_FILE;
	else if (*sp == 'D') {
		entp->ce_type = CVS_ENT_DIR;
		sp++;
	}
	else {
		/* unknown entry, ignore for future expansion */
		entp->ce_type = CVS_ENT_NONE;
		sp++;
	}

	sp++;
	i = 0;
	do {
		dp = strchr(sp, CVS_ENTRIES_DELIM);
		if (dp != NULL)
			*(dp++) = '\0';
		fields[i++] = sp;
		sp = dp;
	} while ((dp != NULL) && (i < CVS_ENTRIES_NFIELDS));

	entp->ce_name = fields[0];

	if (entp->ce_type == CVS_ENT_FILE) {
		rcsnum_aton(fields[1], NULL, entp->ce_rev);
		entp->ce_timestamp = fields[2];
		entp->ce_opts = fields[3];
		entp->ce_tag = fields[4];
	}

	return (entp);
}
