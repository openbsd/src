/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*! \file */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <isc/types.h>
#include <isc/util.h>

#include <dns/fixedname.h>
#include <dns/masterdump.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/rdatatype.h>
#include <dns/result.h>

#define RETERR(x) do { \
	isc_result_t _r = (x); \
	if (_r != ISC_R_SUCCESS) \
		return (_r); \
	} while (0)

struct dns_master_style {
	dns_masterstyle_flags_t flags;		/* DNS_STYLEFLAG_* */
	unsigned int ttl_column;
	unsigned int class_column;
	unsigned int type_column;
	unsigned int rdata_column;
	unsigned int line_length;
	unsigned int tab_width;
	unsigned int split_width;
};

/*%
 * The maximum length of the newline+indentation that is output
 * when inserting a line break in an RR.  This effectively puts an
 * upper limits on the value of "rdata_column", because if it is
 * very large, the tabs and spaces needed to reach it will not fit.
 */
#define DNS_TOTEXT_LINEBREAK_MAXLEN 100

/*%
 * Context structure for a masterfile dump in progress.
 */
typedef struct dns_totext_ctx {
	dns_master_style_t	style;
	int 		class_printed;
	char *			linebreak;
	char 			linebreak_buf[DNS_TOTEXT_LINEBREAK_MAXLEN];
	dns_name_t *		origin;
	dns_name_t *		neworigin;
	dns_fixedname_t		origin_fixname;
	uint32_t 		current_ttl;
	int 		current_ttl_valid;
} dns_totext_ctx_t;

/*%
 * A style suitable for dns_rdataset_totext().
 */
const dns_master_style_t
dns_master_style_debug = {
	DNS_STYLEFLAG_REL_OWNER,
	24, 32, 40, 48, 80, 8, UINT_MAX
};

#define N_SPACES 10
static char spaces[N_SPACES+1] = "          ";

#define N_TABS 10
static char tabs[N_TABS+1] = "\t\t\t\t\t\t\t\t\t\t";

/*%
 * Output tabs and spaces to go from column '*current' to
 * column 'to', and update '*current' to reflect the new
 * current column.
 */
static isc_result_t
indent(unsigned int *current, unsigned int to, int tabwidth,
       isc_buffer_t *target)
{
	isc_region_t r;
	unsigned char *p;
	unsigned int from;
	int ntabs, nspaces, t;

	from = *current;

	if (to < from + 1)
		to = from + 1;

	ntabs = to / tabwidth - from / tabwidth;
	if (ntabs < 0)
		ntabs = 0;

	if (ntabs > 0) {
		isc_buffer_availableregion(target, &r);
		if (r.length < (unsigned) ntabs)
			return (ISC_R_NOSPACE);
		p = r.base;

		t = ntabs;
		while (t) {
			int n = t;
			if (n > N_TABS)
				n = N_TABS;
			memmove(p, tabs, n);
			p += n;
			t -= n;
		}
		isc_buffer_add(target, ntabs);
		from = (to / tabwidth) * tabwidth;
	}

	nspaces = to - from;
	INSIST(nspaces >= 0);

	isc_buffer_availableregion(target, &r);
	if (r.length < (unsigned) nspaces)
		return (ISC_R_NOSPACE);
	p = r.base;

	t = nspaces;
	while (t) {
		int n = t;
		if (n > N_SPACES)
			n = N_SPACES;
		memmove(p, spaces, n);
		p += n;
		t -= n;
	}
	isc_buffer_add(target, nspaces);

	*current = to;
	return (ISC_R_SUCCESS);
}

static isc_result_t
totext_ctx_init(const dns_master_style_t *style, dns_totext_ctx_t *ctx) {
	isc_result_t result;

	REQUIRE(style->tab_width != 0);

	ctx->style = *style;
	ctx->class_printed = 0;

	dns_fixedname_init(&ctx->origin_fixname);

	/*
	 * Set up the line break string if needed.
	 */
	if ((ctx->style.flags & DNS_STYLEFLAG_MULTILINE) != 0) {
		isc_buffer_t buf;
		isc_region_t r;
		unsigned int col = 0;

		isc_buffer_init(&buf, ctx->linebreak_buf,
				sizeof(ctx->linebreak_buf));

		isc_buffer_availableregion(&buf, &r);
		if (r.length < 1)
			return (DNS_R_TEXTTOOLONG);
		r.base[0] = '\n';
		isc_buffer_add(&buf, 1);

		if ((ctx->style.flags & DNS_STYLEFLAG_COMMENTDATA) != 0) {
			isc_buffer_availableregion(&buf, &r);
			if (r.length < 1)
				return (DNS_R_TEXTTOOLONG);
			r.base[0] = ';';
			isc_buffer_add(&buf, 1);
		}

		result = indent(&col, ctx->style.rdata_column,
				ctx->style.tab_width, &buf);
		/*
		 * Do not return ISC_R_NOSPACE if the line break string
		 * buffer is too small, because that would just make
		 * dump_rdataset() retry indefinitely with ever
		 * bigger target buffers.  That's a different buffer,
		 * so it won't help.  Use DNS_R_TEXTTOOLONG as a substitute.
		 */
		if (result == ISC_R_NOSPACE)
			return (DNS_R_TEXTTOOLONG);
		if (result != ISC_R_SUCCESS)
			return (result);

		isc_buffer_availableregion(&buf, &r);
		if (r.length < 1)
			return (DNS_R_TEXTTOOLONG);
		r.base[0] = '\0';
		isc_buffer_add(&buf, 1);
		ctx->linebreak = ctx->linebreak_buf;
	} else {
		ctx->linebreak = NULL;
	}

	ctx->origin = NULL;
	ctx->neworigin = NULL;
	ctx->current_ttl = 0;
	ctx->current_ttl_valid = 0;

	return (ISC_R_SUCCESS);
}

#define INDENT_TO(col) \
	do { \
		 if ((result = indent(&column, ctx->style.col, \
				      ctx->style.tab_width, target)) \
		     != ISC_R_SUCCESS) \
			    return (result); \
	} while (0)

/*
 * Convert 'rdataset' to master file text format according to 'ctx',
 * storing the result in 'target'.  If 'owner_name' is NULL, it
 * is omitted; otherwise 'owner_name' must be valid and have at least
 * one label.
 */

static isc_result_t
rdataset_totext(dns_rdataset_t *rdataset,
		dns_name_t *owner_name,
		dns_totext_ctx_t *ctx,
		int omit_final_dot,
		isc_buffer_t *target)
{
	isc_result_t result;
	unsigned int column;
	int first = 1;
	uint32_t current_ttl;
	int current_ttl_valid;
	dns_rdatatype_t type;
	unsigned int type_start;

	result = dns_rdataset_first(rdataset);

	current_ttl = ctx->current_ttl;
	current_ttl_valid = ctx->current_ttl_valid;

	while (result == ISC_R_SUCCESS) {
		column = 0;

		/*
		 * Comment?
		 */
		if ((ctx->style.flags & DNS_STYLEFLAG_COMMENTDATA) != 0)
			RETERR(isc_str_tobuffer(";", target));

		/*
		 * Owner name.
		 */
		if (owner_name != NULL &&
		    ! ((ctx->style.flags & DNS_STYLEFLAG_OMIT_OWNER) != 0 &&
		       !first))
		{
			unsigned int name_start = target->used;
			RETERR(dns_name_totext(owner_name,
					       omit_final_dot,
					       target));
			column += target->used - name_start;
		}

		/*
		 * TTL.
		 */
		if ((ctx->style.flags & DNS_STYLEFLAG_NO_TTL) == 0 &&
		    !((ctx->style.flags & DNS_STYLEFLAG_OMIT_TTL) != 0 &&
		      current_ttl_valid &&
		      rdataset->ttl == current_ttl))
		{
			char ttlbuf[64];
			isc_region_t r;
			unsigned int length;

			INDENT_TO(ttl_column);
			length = snprintf(ttlbuf, sizeof(ttlbuf), "%u",
					  rdataset->ttl);
			INSIST(length <= sizeof(ttlbuf));
			isc_buffer_availableregion(target, &r);
			if (r.length < length)
				return (ISC_R_NOSPACE);
			memmove(r.base, ttlbuf, length);
			isc_buffer_add(target, length);
			column += length;

			/*
			 * If the $TTL directive is not in use, the TTL we
			 * just printed becomes the default for subsequent RRs.
			 */
			if ((ctx->style.flags & DNS_STYLEFLAG_TTL) == 0) {
				current_ttl = rdataset->ttl;
				current_ttl_valid = 1;
			}
		}

		/*
		 * Class.
		 */
		if ((ctx->style.flags & DNS_STYLEFLAG_NO_CLASS) == 0 &&
		    ((ctx->style.flags & DNS_STYLEFLAG_OMIT_CLASS) == 0 ||
		     !ctx->class_printed))
		{
			unsigned int class_start;
			INDENT_TO(class_column);
			class_start = target->used;
			result = dns_rdataclass_totext(rdataset->rdclass,
						       target);
			if (result != ISC_R_SUCCESS)
				return (result);
			column += (target->used - class_start);
		}

		/*
		 * Type.
		 */

		type = rdataset->type;

		INDENT_TO(type_column);
		type_start = target->used;
		switch (type) {
		case dns_rdatatype_keydata:
#define KEYDATA "KEYDATA"
			if ((ctx->style.flags & DNS_STYLEFLAG_KEYDATA) != 0) {
				if (isc_buffer_availablelength(target) <
				    (sizeof(KEYDATA) - 1))
					return (ISC_R_NOSPACE);
				isc_buffer_putstr(target, KEYDATA);
				break;
			}
			/* FALLTHROUGH */
		default:
			result = dns_rdatatype_totext(type, target);
			if (result != ISC_R_SUCCESS)
				return (result);
		}
		column += (target->used - type_start);

		/*
		 * Rdata.
		 */
		INDENT_TO(rdata_column);
		{
			dns_rdata_t rdata = DNS_RDATA_INIT;
			isc_region_t r;

			dns_rdataset_current(rdataset, &rdata);

			RETERR(dns_rdata_tofmttext(&rdata,
						   ctx->origin,
						   (unsigned int)
						   ctx->style.flags,
						   ctx->style.line_length -
						       ctx->style.rdata_column,
						   ctx->style.split_width,
						   ctx->linebreak,
						   target));

			isc_buffer_availableregion(target, &r);
			if (r.length < 1)
				return (ISC_R_NOSPACE);
			r.base[0] = '\n';
			isc_buffer_add(target, 1);
		}

		first = 0;
		result = dns_rdataset_next(rdataset);
	}

	if (result != ISC_R_NOMORE)
		return (result);

	/*
	 * Update the ctx state to reflect what we just printed.
	 * This is done last, only when we are sure we will return
	 * success, because this function may be called multiple
	 * times with increasing buffer sizes until it succeeds,
	 * and failed attempts must not update the state prematurely.
	 */
	ctx->class_printed = 1;
	ctx->current_ttl= current_ttl;
	ctx->current_ttl_valid = current_ttl_valid;

	return (ISC_R_SUCCESS);
}

/*
 * Print the name, type, and class of an empty rdataset,
 * such as those used to represent the question section
 * of a DNS message.
 */
static isc_result_t
question_totext(dns_rdataset_t *rdataset,
		dns_name_t *owner_name,
		dns_totext_ctx_t *ctx,
		int omit_final_dot,
		isc_buffer_t *target)
{
	unsigned int column;
	isc_result_t result;
	isc_region_t r;

	result = dns_rdataset_first(rdataset);
	REQUIRE(result == ISC_R_NOMORE);

	column = 0;

	/* Owner name */
	{
		unsigned int name_start = target->used;
		RETERR(dns_name_totext(owner_name,
				       omit_final_dot,
				       target));
		column += target->used - name_start;
	}

	/* Class */
	{
		unsigned int class_start;
		INDENT_TO(class_column);
		class_start = target->used;
		result = dns_rdataclass_totext(rdataset->rdclass, target);
		if (result != ISC_R_SUCCESS)
			return (result);
		column += (target->used - class_start);
	}

	/* Type */
	{
		unsigned int type_start;
		INDENT_TO(type_column);
		type_start = target->used;
		result = dns_rdatatype_totext(rdataset->type, target);
		if (result != ISC_R_SUCCESS)
			return (result);
		column += (target->used - type_start);
	}

	isc_buffer_availableregion(target, &r);
	if (r.length < 1)
		return (ISC_R_NOSPACE);
	r.base[0] = '\n';
	isc_buffer_add(target, 1);

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_rdataset_totext(dns_rdataset_t *rdataset,
		    dns_name_t *owner_name,
		    int omit_final_dot,
		    int question,
		    isc_buffer_t *target)
{
	dns_totext_ctx_t ctx;
	isc_result_t result;
	result = totext_ctx_init(&dns_master_style_debug, &ctx);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "could not set master file style");
		return (ISC_R_UNEXPECTED);
	}

	/*
	 * The caller might want to give us an empty owner
	 * name (e.g. if they are outputting into a master
	 * file and this rdataset has the same name as the
	 * previous one.)
	 */
	if (dns_name_countlabels(owner_name) == 0)
		owner_name = NULL;

	if (question)
		return (question_totext(rdataset, owner_name, &ctx,
					omit_final_dot, target));
	else
		return (rdataset_totext(rdataset, owner_name, &ctx,
					omit_final_dot, target));
}

isc_result_t
dns_master_rdatasettotext(dns_name_t *owner_name,
			  dns_rdataset_t *rdataset,
			  const dns_master_style_t *style,
			  isc_buffer_t *target)
{
	dns_totext_ctx_t ctx;
	isc_result_t result;
	result = totext_ctx_init(style, &ctx);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "could not set master file style");
		return (ISC_R_UNEXPECTED);
	}

	return (rdataset_totext(rdataset, owner_name, &ctx,
				0, target));
}

isc_result_t
dns_master_questiontotext(dns_name_t *owner_name,
			  dns_rdataset_t *rdataset,
			  const dns_master_style_t *style,
			  isc_buffer_t *target)
{
	dns_totext_ctx_t ctx;
	isc_result_t result;
	result = totext_ctx_init(style, &ctx);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "could not set master file style");
		return (ISC_R_UNEXPECTED);
	}

	return (question_totext(rdataset, owner_name, &ctx,
				0, target));
}

isc_result_t
dns_master_stylecreate2(dns_master_style_t **stylep, unsigned int flags,
			unsigned int ttl_column, unsigned int class_column,
			unsigned int type_column, unsigned int rdata_column,
			unsigned int line_length, unsigned int tab_width,
			unsigned int split_width)
{
	dns_master_style_t *style;

	REQUIRE(stylep != NULL && *stylep == NULL);
	style = malloc(sizeof(*style));
	if (style == NULL)
		return (ISC_R_NOMEMORY);

	style->flags = flags;
	style->ttl_column = ttl_column;
	style->class_column = class_column;
	style->type_column = type_column;
	style->rdata_column = rdata_column;
	style->line_length = line_length;
	style->tab_width = tab_width;
	style->split_width = split_width;

	*stylep = style;
	return (ISC_R_SUCCESS);
}

void
dns_master_styledestroy(dns_master_style_t **stylep) {
	dns_master_style_t *style;

	REQUIRE(stylep != NULL && *stylep != NULL);
	style = *stylep;
	*stylep = NULL;
	free(style);
}
