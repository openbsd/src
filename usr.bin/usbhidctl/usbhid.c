/*	$OpenBSD: usbhid.c,v 1.2 2001/12/30 07:10:42 pvalchev Exp $	*/
/*      $NetBSD: usbhid.c,v 1.17 2001/03/28 03:17:42 simonb Exp $ */

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Sainty <David.Sainty@dtsp.co.nz>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usbhid.h>

/* Parser tokens */
#define DELIM_USAGE '.'
#define DELIM_PAGE ':'
#define DELIM_SET '='

/* Zero if not in a verbose mode.  Greater levels of verbosity are
   indicated by values larger than one. */
static unsigned int verbose;

struct Susbvar {
	/* Variable name, not NUL terminated */
	char const *variable;
	size_t varlen;

	char const *value; /* Value to set variable to */

#define MATCH_ALL		(1 << 0)
#define MATCH_COLLECTIONS	(1 << 1)
#define MATCH_NODATA		(1 << 2)
#define MATCH_CONSTANTS		(1 << 3)
#define MATCH_WASMATCHED	(1 << 4)
#define MATCH_SHOWPAGENAME	(1 << 5)
#define MATCH_SHOWNUMERIC	(1 << 6)
#define MATCH_WRITABLE		(1 << 7)
	unsigned int mflags;

	/* Workspace for hidmatch() */
	ssize_t matchindex;

	int (*opfunc)(struct hid_item *item, struct Susbvar *var,
		      u_int32_t const *collist, size_t collen, u_char *buf);
};

struct Sreport {
	struct usb_ctl_report *buffer;

	enum {srs_uninit, srs_clean, srs_dirty} status;
	int report_id;
	size_t size;
};

static struct {
	int uhid_report;
	hid_kind_t hid_kind;
	char const *name;
} const reptoparam[] = {
#define REPORT_INPUT 0
	{ UHID_INPUT_REPORT, hid_input, "input" },
#define REPORT_OUTPUT 1
	{ UHID_OUTPUT_REPORT, hid_output, "output" },
#define REPORT_FEATURE 2
	{ UHID_FEATURE_REPORT, hid_feature, "feature" }
#define REPORT_MAXVAL 2
};

static struct Susbvar*
hidmatch(u_int32_t const *collist, size_t collen, struct hid_item *item,
	 struct Susbvar *varlist, size_t vlsize)
{
	size_t vlind, colind, vlactive;
	int iscollection;

	/*
	 * Keep track of how many variables are still "active".  When
	 * the active count reaches zero, don't bother to continue
	 * looking for matches.
	 */
	vlactive = vlsize;

	iscollection = item->kind == hid_collection ||
		item->kind == hid_endcollection;

	for (vlind = 0; vlind < vlsize; vlind++) {
		struct Susbvar *var;

		var = &varlist[vlind];

		var->matchindex = 0;

		if (!(var->mflags & MATCH_COLLECTIONS) && iscollection) {
			/* Don't match collections for this variable */
			var->matchindex = -1;
			vlactive--;
		} else if (!iscollection && !(var->mflags & MATCH_CONSTANTS) &&
			   (item->flags & HIO_CONST)) {
			/*
			 * Don't match constants for this variable,
			 * but ignore the constant bit on collections.
			 */
			var->matchindex = -1;
			vlactive--;
		} else if ((var->mflags & MATCH_WRITABLE) &&
			   ((item->kind != hid_output &&
			     item->kind != hid_feature) ||
			    (item->flags & HIO_CONST))) {
			/*
			 * If we are only matching writable items, if
			 * this is not an output or feature kind, or
			 * it is a constant, reject it.
			 */
			var->matchindex = -1;
			vlactive--;
		} else if (var->mflags & MATCH_ALL) {
			/* Match immediately */
			return &varlist[vlind];
		}
	}

	for (colind = 0; vlactive > 0 && colind <= collen; colind++) {
		char const *usage_name, *page_name;
		size_t usage_len, page_len;
		int final;
		u_int32_t usage_id;

		final = (colind == collen);

		if (final)
			usage_id = item->usage;
		else
			usage_id = collist[colind];

		usage_name = hid_usage_in_page(usage_id);
		usage_len = strlen(usage_name);

		page_name = NULL;

		for (vlind = 0; vlind < vlsize; vlind++) {
			ssize_t matchindex, pagesplit;
			size_t varlen, strind;
			char const *varname;
			struct Susbvar *var;

			var = &varlist[vlind];

			matchindex = var->matchindex;
			varname = var->variable;
			varlen = var->varlen;

			if (matchindex < 0)
				/* Mismatch at a previous level */
				continue;

			pagesplit = -1;
			for (strind = matchindex; strind < varlen; strind++) {
				if (varname[strind] == DELIM_USAGE)
					break;
				if (varname[strind] == DELIM_PAGE)
					pagesplit = strind;
			}

			if (final && strind != varlen) {
				/*
				 * Variable name is too long (hit
				 * delimiter instead of
				 * end-of-variable)
				 */
				var->matchindex = -1;
				vlactive--;
				continue;
			}

			if (pagesplit >= 0) {
				if (page_name == NULL) {
					page_name = hid_usage_page(HID_PAGE(usage_id));
					page_len = strlen(page_name);
				}
				if (page_len !=
				    (size_t)(pagesplit - matchindex) ||
				    memcmp(page_name, &varname[matchindex],
					   page_len) != 0) {
					/* Mismatch, page name wrong */
					var->matchindex = -1;
					vlactive--;
					continue;
				}

				/* Page matches, discard page name */
				matchindex = pagesplit + 1;
			}

			if (usage_len != strind - matchindex ||
			    memcmp(usage_name, &varname[matchindex],
				   usage_len) != 0) {
				/* Mismatch, usage name wrong */
				var->matchindex = -1;
				vlactive--;
				continue;
			}

			if (final)
				/* Match */
				return var;

			/*
			 * Partial match: Move index past this usage
			 * string + delimiter
			 */
			var->matchindex = matchindex + usage_len + 1;
		}
	}

	return NULL;
}

static void
allocreport(struct Sreport *report, report_desc_t rd, int repindex)
{
	int reptsize;

	reptsize = hid_report_size(rd, reptoparam[repindex].hid_kind,
				   &report->report_id);
	if (reptsize < 0)
		errx(1, "Negative report size");
	report->size = reptsize;

	if (report->size > 0) {
		/*
		 * Allocate a buffer with enough space for the
		 * report in the variable-sized data field.
		 */
		report->buffer = malloc(sizeof(*report->buffer) -
					sizeof(report->buffer->data) +
					report->size);
		if (report->buffer == NULL)
			err(1, NULL);
	} else
		report->buffer = NULL;

	report->status = srs_clean;
}

static void
freereport(struct Sreport *report)
{
	if (report->buffer != NULL)
		free(report->buffer);
	report->status = srs_uninit;
}

static void
getreport(struct Sreport *report, int hidfd, report_desc_t rd, int repindex)
{
	if (report->status == srs_uninit) {
		allocreport(report, rd, repindex);
		if (report->size == 0)
			return;

		report->buffer->report = reptoparam[repindex].uhid_report;
		if (ioctl(hidfd, USB_GET_REPORT, report->buffer) < 0)
			err(1, "USB_GET_REPORT");
	}
}

static void
setreport(struct Sreport *report, int hidfd, int repindex)
{
	if (report->status == srs_dirty) {
		report->buffer->report = reptoparam[repindex].uhid_report;

		if (ioctl(hidfd, USB_SET_REPORT, report->buffer) < 0)
			err(1, "USB_SET_REPORT(%s)",
			    reptoparam[repindex].name);

		report->status = srs_clean;
	}
}

/* ARGSUSED1 */
static int
varop_value(struct hid_item *item, struct Susbvar *var,
	    u_int32_t const *collist, size_t collen, u_char *buf)
{
	printf("%d\n", hid_get_data(buf, item));
	return 0;
}

/* ARGSUSED1 */
static int
varop_display(struct hid_item *item, struct Susbvar *var,
	      u_int32_t const *collist, size_t collen, u_char *buf)
{
	size_t colitem;

	for (colitem = 0; colitem < collen; colitem++) {
		if (var->mflags & MATCH_SHOWPAGENAME)
			printf("%s:",
			       hid_usage_page(HID_PAGE(collist[colitem])));
		printf("%s.", hid_usage_in_page(collist[colitem]));
	}

	if (var->mflags & MATCH_SHOWPAGENAME)
		printf("%s:", hid_usage_page(HID_PAGE(item->usage)));
	printf("%s=%d%s\n", hid_usage_in_page(item->usage),
	       hid_get_data(buf, item),
	       (item->flags & HIO_CONST) ? " (const)" : "");
	return 0;
}

/* ARGSUSED1 */
static int
varop_modify(struct hid_item *item, struct Susbvar *var,
	     u_int32_t const *collist, size_t collen, u_char *buf)
{
	u_int dataval;

	dataval = (u_int)strtol(var->value, NULL, 10);

	hid_set_data(buf, item, dataval);

	if (verbose >= 1)
		/*
		 * Allow displaying of set value in verbose mode.
		 * This isn't particularly useful though, so don't
		 * bother documenting it.
		 */
		varop_display(item, var, collist, collen, buf);

	return 1;
}

static void
reportitem(char const *label, struct hid_item const *item, unsigned int mflags)
{
	printf("%s size=%d count=%d page=%s usage=%s%s", label,
	       item->report_size, item->report_count,
	       hid_usage_page(HID_PAGE(item->usage)),
	       hid_usage_in_page(item->usage),
	       item->flags & HIO_CONST ? " Const" : "");
	if (mflags & MATCH_SHOWNUMERIC)
		printf(" (%u/0x%x)",
		       HID_PAGE(item->usage), HID_USAGE(item->usage));
	printf(", logical range %d..%d",
	       item->logical_minimum, item->logical_maximum);
	if (item->physical_minimum != item->physical_maximum)
		printf(", physical range %d..%d",
		       item->physical_minimum, item->physical_maximum);
	if (item->unit)
		printf(", unit=0x%02x exp=%d", item->unit,
		       item->unit_exponent);
	printf("\n");
}

/* ARGSUSED1 */
static int
varop_report(struct hid_item *item, struct Susbvar *var,
	     u_int32_t const *collist, size_t collen, u_char *buf)
{
	switch (item->kind) {
	case hid_collection:
		printf("Collection page=%s usage=%s\n",
		       hid_usage_page(HID_PAGE(item->usage)),
		       hid_usage_in_page(item->usage));
		break;
	case hid_endcollection:
		printf("End collection\n");
		break;
	case hid_input:
		reportitem("Input  ", item, var->mflags);
		break;
	case hid_output:
		reportitem("Output ", item, var->mflags);
		break;
	case hid_feature:
		reportitem("Feature", item, var->mflags);
		break;
	}

	return 0;
}

static void
devloop(int hidfd, report_desc_t rd, struct Susbvar *varlist, size_t vlsize)
{
	struct hid_data *hdata;
	struct hid_item hitem;
	u_int32_t colls[128];
	struct Sreport inreport;
	size_t dlen;
	u_char *dbuf;
	size_t collind;

	allocreport(&inreport, rd, REPORT_INPUT);

	if (inreport.size <= 0)
		errx(1, "Input report descriptor invalid length");

	dlen = inreport.size;
	dbuf = inreport.buffer->data;

	for (;;) {
		ssize_t readlen;

		readlen = read(hidfd, dbuf, dlen);
		if (readlen < 0 || dlen != (size_t)readlen)
			err(1, "bad read %ld != %ld",
			    (long)readlen, (long)dlen);

		collind = 0;
		hdata = hid_start_parse(rd, 1 << hid_input);
		if (hdata == NULL)
			errx(1, "Failed to start parser");

		while (hid_get_item(hdata, &hitem)) {
			struct Susbvar *matchvar;

			switch (hitem.kind) {
			case hid_collection:
				if (collind >= (sizeof(colls) / sizeof(*colls)))
					errx(1, "Excessive nested collections");
				colls[collind++] = hitem.usage;
				break;
			case hid_endcollection:
				if (collind == 0)
					errx(1, "Excessive collection ends");
				collind--;
				break;
			case hid_input:
				break;
			case hid_output:
			case hid_feature:
				errx(1, "Unexpected non-input item returned");
			}

			matchvar = hidmatch(colls, collind, &hitem,
					    varlist, vlsize);

			if (matchvar != NULL)
				matchvar->opfunc(&hitem, matchvar,
						 colls, collind,
						 inreport.buffer->data);
		}
		hid_end_parse(hdata);
		printf("\n");
	}
	/* NOTREACHED */
}

static void
devshow(int hidfd, report_desc_t rd, struct Susbvar *varlist, size_t vlsize,
	int kindset)
{
	struct hid_data *hdata;
	struct hid_item hitem;
	u_int32_t colls[128];
	size_t collind, repind, vlind;

	struct Sreport reports[REPORT_MAXVAL + 1];


	for (repind = 0; repind < (sizeof(reports) / sizeof(*reports));
	     repind++) {
		reports[repind].status = srs_uninit;
		reports[repind].buffer = NULL;
	}

	collind = 0;
	hdata = hid_start_parse(rd, kindset |
				(1 << hid_collection) |
				(1 << hid_endcollection));
	if (hdata == NULL)
		errx(1, "Failed to start parser");

	while (hid_get_item(hdata, &hitem)) {
		struct Susbvar *matchvar;
		int repindex;

		repindex = -1;
		switch (hitem.kind) {
		case hid_collection:
			if (collind >= (sizeof(colls) / sizeof(*colls)))
				errx(1, "Excessive nested collections");
			colls[collind++] = hitem.usage;
			break;
		case hid_endcollection:
			if (collind == 0)
				errx(1, "Excessive collection ends");
			collind--;
			break;
		case hid_input:
			repindex = REPORT_INPUT;
			break;
		case hid_output:
			repindex = REPORT_OUTPUT;
			break;
		case hid_feature:
			repindex = REPORT_FEATURE;
			break;
		}

		matchvar = hidmatch(colls, collind, &hitem, varlist, vlsize);

		if (matchvar != NULL) {
			u_char *bufdata;
			struct Sreport *repptr;

			matchvar->mflags |= MATCH_WASMATCHED;

			if (repindex >= 0)
				repptr = &reports[repindex];
			else
				repptr = NULL;

			if (repptr != NULL &&
			    !(matchvar->mflags & MATCH_NODATA))
				getreport(repptr, hidfd, rd, repindex);

			bufdata = (repptr == NULL || repptr->buffer == NULL) ?
				NULL : repptr->buffer->data;

			if (matchvar->opfunc(&hitem, matchvar, colls, collind,
					     bufdata))
				repptr->status = srs_dirty;
		}
	}
	hid_end_parse(hdata);

	for (repind = 0; repind < (sizeof(reports) / sizeof(*reports));
	     repind++) {
		setreport(&reports[repind], hidfd, repind);
		freereport(&reports[repind]);
	}

	/* Warn about any items that we couldn't find a match for */
	for (vlind = 0; vlind < vlsize; vlind++) {
		struct Susbvar *var;

		var = &varlist[vlind];

		if (var->variable != NULL &&
		    !(var->mflags & MATCH_WASMATCHED))
			warnx("Failed to match: %.*s", (int)var->varlen,
			      var->variable);
	}
}

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "Usage: %s -f device [-t tablefile] [-l] [-v] -a\n",
	    __progname);
	fprintf(stderr, "       %s -f device [-t tablefile] [-v] -r\n",
	    __progname);
	fprintf(stderr,
	    "       %s -f device [-t tablefile] [-l] [-n] [-v] name ...\n",
	    __progname);
	fprintf(stderr,
	    "       %s -f device [-t tablefile] -w name=value ...\n",
	    __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int hidfd;
	report_desc_t repdesc;
	char devnamebuf[PATH_MAX];
	char const *dev;
	int ch, wflag, aflag, nflag, rflag, lflag;
	size_t varnum;
	char const *table;
	struct Susbvar variables[128];

	wflag = aflag = nflag = verbose = rflag = lflag = 0;
	dev = NULL;
	table = NULL;
	while ((ch = getopt(argc, argv, "?af:lnrt:vw")) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'f':
			dev = optarg;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 't':
			table = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			wflag = 1;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (dev == NULL || (lflag && (wflag || rflag))) {
		/*
		 * No device specified, or attempting to loop and set
		 * or dump report at the same time
		 */
		usage();
		/* NOTREACHED */
	}

	for (varnum = 0; varnum < (size_t)argc; varnum++) {
		char const *name, *valuesep;
		struct Susbvar *svar;

		svar = &variables[varnum];
		name = argv[varnum];
		valuesep = strchr(name, DELIM_SET);

		svar->variable = name;
		svar->mflags = 0;

		if (valuesep == NULL) {
			/* Read variable */
			if (wflag)
				errx(1, "Must not specify -w to read variables");
			svar->value = NULL;
			svar->varlen = strlen(name);

			if (nflag) {
				/* Display value of variable only */
				svar->opfunc = varop_value;
			} else {
				/* Display name and value of variable */
				svar->opfunc = varop_display;

				if (verbose >= 1)
					/* Show page names in verbose modes */
					svar->mflags |= MATCH_SHOWPAGENAME;
			}
		} else {
			/* Write variable */
			if (!wflag)
				errx(2, "Must specify -w to set variables");
			svar->mflags |= MATCH_WRITABLE;
			svar->varlen = valuesep - name;
			svar->value = valuesep + 1;
			svar->opfunc = varop_modify;
		}
	}

	if (aflag || rflag) {
		struct Susbvar *svar;

		svar = &variables[varnum++];

		svar->variable = NULL;
		svar->mflags = MATCH_ALL;

		if (rflag) {
			/*
			 * Dump report descriptor.  Do dump collection
			 * items also, and hint that it won't be
			 * necessary to get the item status.
			 */
			svar->opfunc = varop_report;
			svar->mflags |= MATCH_COLLECTIONS | MATCH_NODATA;

			switch (verbose) {
			default:
				/* Level 2: Show item numerics and constants */
				svar->mflags |= MATCH_SHOWNUMERIC;
				/* FALLTHROUGH */
			case 1:
				/* Level 1: Just show constants */
				svar->mflags |= MATCH_CONSTANTS;
				/* FALLTHROUGH */
			case 0:
				break;
			}
		} else {
			/* Display name and value of variable */
			svar->opfunc = varop_display;

			switch (verbose) {
			default:
				/* Level 2: Show constants and page names */
				svar->mflags |= MATCH_CONSTANTS;
				/* FALLTHROUGH */
			case 1:
				/* Level 1: Just show page names */
				svar->mflags |= MATCH_SHOWPAGENAME;
				/* FALLTHROUGH */
			case 0:
				break;
			}
		}
	}

	if (varnum == 0) {
		/* Nothing to do...  Display usage information. */
		usage();
		/* NOTREACHED */
	}

	hid_init(table);

	if (dev[0] != '/') {
		snprintf(devnamebuf, sizeof(devnamebuf), "/dev/%s%s",
			 isdigit(dev[0]) ? "uhid" : "", dev);
		dev = devnamebuf;
	}

	hidfd = open(dev, O_RDWR);
	if (hidfd < 0)
		err(1, "%s", dev);

	repdesc = hid_get_report_desc(hidfd);
	if (repdesc == 0)
		errx(1, "USB_GET_REPORT_DESC");

	if (lflag) {
		devloop(hidfd, repdesc, variables, varnum);
		/* NOTREACHED */
	}

	if (rflag)
		/* Report mode header */
		printf("Report descriptor:\n");

	devshow(hidfd, repdesc, variables, varnum,
		1 << hid_input |
		1 << hid_output |
		1 << hid_feature);

#if 0
	{
		size_t repindex;
		for (repindex = 0;
		     repindex < (sizeof(reptoparam) / sizeof(*reptoparam));
		     repindex++)
			devshow(hidfd, repdesc, variables, varnum,
				1 << reptoparam[repindex].hid_kind);
	}
#endif

	if (rflag) {
		/* Report mode trailer */
		size_t repindex;
		for (repindex = 0;
		     repindex < (sizeof(reptoparam) / sizeof(*reptoparam));
		     repindex++) {
			int report_id, size;
			size = hid_report_size(repdesc,
					       reptoparam[repindex].hid_kind,
					       &report_id);
			size -= report_id != 0;
			printf("Total %7s size %s%d bytes\n",
			       reptoparam[repindex].name,
			       report_id && size ? "1+" : "", size);
		}
	}

	hid_dispose_report_desc(repdesc);
	exit(0);
	/* NOTREACHED */
}
