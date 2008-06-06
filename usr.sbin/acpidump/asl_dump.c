/*	$OpenBSD: asl_dump.c,v 1.8 2008/06/06 10:16:47 marco Exp $	*/
/*-
 * Copyright (c) 1999 Doug Rabson
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: asl_dump.c,v 1.8 2008/06/06 10:16:47 marco Exp $
 *	$FreeBSD: src/usr.sbin/acpi/acpidump/asl_dump.c,v 1.5 2001/10/23 14:53:58 takawata Exp $
 */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <stdio.h>

#include "acpidump.h"

#include "aml/aml_env.h"

struct aml_environ	asl_env;

static u_int32_t
asl_dump_pkglength(u_int8_t **dpp)
{
	u_int8_t	*dp;
	u_int32_t	pkglength;

	dp = *dpp;
	pkglength = *dp++;
	switch (pkglength >> 6) {
	case 0:
		break;
	case 1:
		pkglength = (pkglength & 0xf) + (dp[0] << 4);
		dp += 1;
		break;
	case 2:
		pkglength = (pkglength & 0xf) + (dp[0] << 4) + (dp[1] << 12);
		dp += 2;
		break;
	case 3:
		pkglength = (pkglength & 0xf)
			+ (dp[0] << 4) + (dp[1] << 12) + (dp[2] << 20);
		dp += 3;
		break;
	}

	*dpp = dp;
	return (pkglength);
}

static void
print_nameseg(u_int8_t *dp)
{

	if (dp[3] != '_')
		printf("%c%c%c%c", dp[0], dp[1], dp[2], dp[3]);
	else if (dp[2] != '_')
		printf("%c%c%c_", dp[0], dp[1], dp[2]);
	else if (dp[1] != '_')
		printf("%c%c__", dp[0], dp[1]);
	else if (dp[0] != '_')
		printf("%c___", dp[0]);
}

static u_int8_t
asl_dump_bytedata(u_int8_t **dpp)
{
	u_int8_t	*dp;
	u_int8_t	data;

	dp = *dpp;
	data = dp[0];
	*dpp = dp + 1;
	return (data);
}

static u_int16_t
asl_dump_worddata(u_int8_t **dpp)
{
	u_int8_t	*dp;
	u_int16_t	data;

	dp = *dpp;
	data = dp[0] + (dp[1] << 8);
	*dpp = dp + 2;
	return (data);
}

static u_int32_t
asl_dump_dworddata(u_int8_t **dpp)
{
	u_int8_t	*dp;
	u_int32_t	data;

	dp = *dpp;
	data = dp[0] + (dp[1] << 8) + (dp[2] << 16) + (dp[3] << 24);
	*dpp = dp + 4;
	return (data);
}

static u_int8_t *
asl_dump_namestring(u_int8_t **dpp)
{
	u_int8_t	*dp;
	u_int8_t	*name;

	dp = *dpp;
	name = dp;
	if (dp[0] == '\\')
		dp++;
	else if (dp[0] == '^')
		while (dp[0] == '^')
			dp++;
	if (dp[0] == 0x00)	/* NullName */
		dp++;
	else if (dp[0] == 0x2e)	/* DualNamePrefix */
		dp += 1 + 4 + 4;/* NameSeg, NameSeg */
	else if (dp[0] == 0x2f) {	/* MultiNamePrefix */
		int             segcount = dp[1];
		dp += 1 + 1 + segcount * 4;	/* segcount * NameSeg */
	} else
		dp += 4;	/* NameSeg */

	*dpp = dp;
	return (name);
}

static void
print_namestring(u_int8_t *dp)
{

	if (dp[0] == '\\') {
		putchar(dp[0]);
		dp++;
	} else if (dp[0] == '^') {
		while (dp[0] == '^') {
			putchar(dp[0]);
			dp++;
		}
	}
	if (dp[0] == 0x00) {	/* NullName */
		/* printf("<null>"); */
		dp++;
	} else if (dp[0] == 0x2e) {	/* DualNamePrefix */
		print_nameseg(dp + 1);
		putchar('.');
		print_nameseg(dp + 5);
	} else if (dp[0] == 0x2f) {	/* MultiNamePrefix */
		int             segcount = dp[1];
		int             i;
		for (i = 0, dp += 2; i < segcount; i++, dp += 4) {
			if (i > 0)
				putchar('.');
			print_nameseg(dp);
		}
	} else			/* NameSeg */
		print_nameseg(dp);
}

static void
print_indent(int indent)
{
	int	i;

	for (i = 0; i < indent; i++)
		printf("    ");
}

#define ASL_ENTER_SCOPE(dp_orig, old_name) do {				\
	u_int8_t	*dp_copy;					\
	u_int8_t	*name;						\
	old_name = asl_env.curname;					\
	dp_copy = dp_orig;						\
	name = asl_dump_namestring(&dp_copy);				\
	asl_env.curname = aml_search_name(&asl_env, name);		\
} while(0)

#define ASL_LEAVE_SCOPE(old_name) do {					\
	asl_env.curname = old_name;					\
} while(0)

#define ASL_CREATE_LOCALNAMEOBJ(dp) do {				\
	if(scope_within_method){					\
		aml_create_name(&asl_env, dp);				\
	}								\
}while(0);

static void
asl_dump_defscope(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;
	struct	aml_name *oname;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);

	printf("Scope(");
	ASL_ENTER_SCOPE(dp, oname);
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	end = start + pkglength;
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);
	ASL_LEAVE_SCOPE(oname);
	*dpp = dp;
}

static void
asl_dump_defbuffer(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;
	printf("Buffer(");
	asl_dump_termobj(&dp, indent);
	printf(") {");
	while (dp < end) {
		printf("0x%x", *dp++);
		if (dp < end)
			printf(", ");
	}
	printf(" }");

	*dpp = dp;
}

static void
asl_dump_defpackage(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	numelements;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	numelements = asl_dump_bytedata(&dp);
	end = start + pkglength;
	printf("Package(0x%x) {\n", numelements);
	while (dp < end) {
		print_indent(indent + 1);
		asl_dump_termobj(&dp, indent + 1);
		printf(",\n");
	}

	print_indent(indent);
	printf("}");

	dp = end;

	*dpp = dp;
}

int	scope_within_method = 0;

static void
asl_dump_defmethod(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	flags;
	u_int32_t	pkglength;
	struct	aml_name *oname;
	int		swi;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);

	swi = scope_within_method;
	scope_within_method = 0;

	printf("Method(");
	ASL_ENTER_SCOPE(dp, oname);
	asl_dump_termobj(&dp, indent);
	flags = *dp++;
	if (flags) {
		printf(", %d", flags & 7);
		if (flags & 8) {
			printf(", Serialized");
		}
	}
	printf(") {\n");
	end = start + pkglength;
	scope_within_method = 1;
	asl_dump_objectlist(&dp, end, indent + 1);
	scope_within_method = swi;
	print_indent(indent);
	printf("}");

	assert(dp == end);
	ASL_LEAVE_SCOPE(oname);
	*dpp = dp;
}


static void
asl_dump_defopregion(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	const	char *regions[] = {
		"SystemMemory",
		"SystemIO",
		"PCI_Config",
		"EmbeddedControl",
		"SMBus",
	};

	dp = *dpp;
	printf("OperationRegion(");
	ASL_CREATE_LOCALNAMEOBJ(dp);
	asl_dump_termobj(&dp, indent);	/* Name */
	printf(", %s, ", regions[*dp++]);	/* Space */
	asl_dump_termobj(&dp, indent);	/* Offset */
	printf(", ");
	asl_dump_termobj(&dp, indent);	/* Length */
	printf(")");

	*dpp = dp;
}

static const char *accessnames[] = {
	"AnyAcc",
	"ByteAcc",
	"WordAcc",
	"DWordAcc",
	"BlockAcc",
	"SMBSendRecvAcc",
	"SMBQuickAcc"
};

static int
asl_dump_field(u_int8_t **dpp, u_int32_t offset)
{
	u_int8_t	*dp;
	u_int8_t	*name;
	u_int8_t	access, attribute;
	u_int32_t	width;

	dp = *dpp;
	switch (*dp) {
	case '\\':
	case '^':
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	case 'G':
	case 'H':
	case 'I':
	case 'J':
	case 'K':
	case 'L':
	case 'M':
	case 'N':
	case 'O':
	case 'P':
	case 'Q':
	case 'R':
	case 'S':
	case 'T':
	case 'U':
	case 'V':
	case 'W':
	case 'X':
	case 'Y':
	case 'Z':
	case '_':
	case '.':
	case '/':
		ASL_CREATE_LOCALNAMEOBJ(dp);
		name = asl_dump_namestring(&dp);
		width = asl_dump_pkglength(&dp);
		offset += width;
		print_namestring(name);
		printf(",\t%d", width);
		break;
	case 0x00:
		dp++;
		width = asl_dump_pkglength(&dp);
		offset += width;
		if ((offset % 8) == 0) {
			printf("Offset(0x%x)", offset / 8);
		} else {
			printf(",\t%d", width);
		}
		break;
	case 0x01:
		access = dp[1];
		attribute = dp[2];
		dp += 3;
		printf("AccessAs(%s, %d)", accessnames[access], attribute);
		break;
	}

	*dpp = dp;
	return (offset);
}

static void
asl_dump_fieldlist(u_int8_t **dpp, u_int8_t *end, int indent)
{
	u_int8_t	*dp;
	u_int32_t	offset;

	dp = *dpp;
	offset = 0;
	while (dp < end) {
		print_indent(indent);
		offset = asl_dump_field(&dp, offset);
		if (dp < end)
			printf(",\n");
		else
			printf("\n");
	}

	*dpp = dp;
}

static void
asl_dump_deffield(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	flags;
	u_int32_t	pkglength;
	static	const char *lockrules[] = {"NoLock", "Lock"};
	static	const char *updaterules[] = {"Preserve", "WriteAsOnes",
					     "WriteAsZeros", "*Error*"};

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("Field(");
	asl_dump_termobj(&dp, indent);	/* Name */
	flags = asl_dump_bytedata(&dp);
	printf(", %s, %s, %s) {\n",
	       accessnames[flags & 0xf],
	       lockrules[(flags >> 4) & 1],
	       updaterules[(flags >> 5) & 3]);
	asl_dump_fieldlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defindexfield(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	flags;
	u_int32_t	pkglength;
	static	const char *lockrules[] = {"NoLock", "Lock"};
	static	const char *updaterules[] = {"Preserve", "WriteAsOnes",
					     "WriteAsZeros", "*Error*"};

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("IndexField(");
	asl_dump_termobj(&dp, indent);	/* Name1 */
	printf(", ");
	asl_dump_termobj(&dp, indent);	/* Name2 */
	flags = asl_dump_bytedata(&dp);
	printf(", %s, %s, %s) {\n",
	       accessnames[flags & 0xf],
	       lockrules[(flags >> 4) & 1],
	       updaterules[(flags >> 5) & 3]);
	asl_dump_fieldlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defbankfield(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	flags;
	u_int32_t	pkglength;
	static	const char *lockrules[] = {"NoLock", "Lock"};
	static	const char *updaterules[] = {"Preserve", "WriteAsOnes",
					     "WriteAsZeros", "*Error*"};

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;
	printf("BankField(");
	asl_dump_termobj(&dp, indent);	/* Name1 */
	printf(", ");
	asl_dump_termobj(&dp, indent);	/* Name2 */
	printf(", ");
	asl_dump_termobj(&dp, indent);	/* BankValue */
	flags = asl_dump_bytedata(&dp);
	printf(", %s, %s, %s) {\n",
	       accessnames[flags & 0xf],
	       lockrules[(flags >> 4) & 1],
	       updaterules[(flags >> 5) & 3]);
	asl_dump_fieldlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defdevice(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;
	struct	aml_name *oname;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("Device(");
	ASL_ENTER_SCOPE(dp, oname);
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	ASL_LEAVE_SCOPE(oname);
	*dpp = dp;
}

static void
asl_dump_defprocessor(u_int8_t **dpp, int indent)
{
	u_int8_t       *dp;
	u_int8_t       *start;
	u_int8_t       *end;
	u_int8_t        procid;
	u_int8_t        pblklen;
	u_int32_t       pkglength;
	u_int32_t       pblkaddr;
	struct	aml_name *oname;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("Processor(");
	ASL_ENTER_SCOPE(dp, oname);
	asl_dump_termobj(&dp, indent);
	procid = asl_dump_bytedata(&dp);
	pblkaddr = asl_dump_dworddata(&dp);
	pblklen = asl_dump_bytedata(&dp);
	printf(", %d, 0x%x, 0x%x) {\n", procid, pblkaddr, pblklen);
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	ASL_LEAVE_SCOPE(oname);
	*dpp = dp;
}

static void
asl_dump_defpowerres(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	systemlevel;
	u_int16_t	resourceorder;
	u_int32_t	pkglength;
	struct	aml_name *oname;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("PowerResource(");
	ASL_ENTER_SCOPE(dp, oname);
	asl_dump_termobj(&dp, indent);
	systemlevel = asl_dump_bytedata(&dp);
	resourceorder = asl_dump_worddata(&dp);
	printf(", %d, %d) {\n", systemlevel, resourceorder);
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	ASL_LEAVE_SCOPE(oname);
	*dpp = dp;
}

static void
asl_dump_defthermalzone(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;
	struct	aml_name *oname;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("ThermalZone(");
	ASL_ENTER_SCOPE(dp, oname);
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	ASL_LEAVE_SCOPE(oname);
	*dpp = dp;
}

static void
asl_dump_defif(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("If(");
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defelse(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t       pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("Else {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defwhile(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("While(");
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_oparg(u_int8_t **dpp, int indent, const char *mnem, 
    const char *fmt)
{
	int idx;
	const char *pfx="";

	printf("%s(", mnem);
	for (idx=0; fmt[idx]; idx++) {
		if (fmt[idx] == 'N') {
			ASL_CREATE_LOCALNAMEOBJ(*dpp);
		}
		else if (fmt[idx] == 'o' && **dpp == 0x00) {
			/* Optional Argument */
			(*dpp)++;
			continue;
		}

		printf(pfx);
		if (fmt[idx] == 'b')
			printf("0x%x", asl_dump_bytedata(dpp));
		else if (fmt[idx] == 'w')
			printf("0x%x", asl_dump_worddata(dpp));
		else if (fmt[idx] == 'd')
			printf("0x%x", asl_dump_dworddata(dpp));
		else
			asl_dump_termobj(dpp, indent);
		pfx = ", ";
	}
	printf(")");
}
/*
 * Public interfaces
 */
void
asl_dump_termobj(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*name;
	u_int8_t	opcode;
	struct	aml_name *method;
	const	char *matchstr[] = {
		"MTR", "MEQ", "MLE", "MLT", "MGE", "MGT",
	};

#define OPTARG() do {						\
	if (*dp == 0x00) {					\
	    dp++;						\
	} else { 						\
	    printf(", ");					\
	    asl_dump_termobj(&dp, indent);			\
	}							\
} while (0)

	dp = *dpp;
	opcode = *dp++;
	switch (opcode) {
	case '\\':
	case '^':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
	case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
	case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
	case 'V': case 'W': case 'X': case 'Y': case 'Z':
	case '_':
	case '.':
	case '/':
		dp--;
		print_namestring((name = asl_dump_namestring(&dp)));
		if (scope_within_method == 1) {
			method = aml_search_name(&asl_env, name);
			if (method != NULL && method->property != NULL &&
			    method->property->type == aml_t_method) {
				int	i, argnum;

				argnum = method->property->meth.argnum & 7;
				printf("(");
				for (i = 0; i < argnum; i++) {
					asl_dump_termobj(&dp, indent);
					if (i < (argnum-1)) {
						printf(", ");
					}
				}
				printf(")");
			}
		}
		break;
	case 0x0a:		/* BytePrefix */
		printf("0x%x", asl_dump_bytedata(&dp));
		break;
	case 0x0b:		/* WordPrefix */
		printf("0x%04x", asl_dump_worddata(&dp));
		break;
	case 0x0c:		/* DWordPrefix */
		printf("0x%08x", asl_dump_dworddata(&dp));
		break;
	case 0x0d:		/* StringPrefix */
		printf("\"%s\"", (const char *) dp);
		while (*dp)
			dp++;
		dp++;		/* NUL terminate */
		break;
	case 0x00:		/* ZeroOp */
		printf("Zero");
		break;
	case 0x01:		/* OneOp */
		printf("One");
		break;
	case 0xff:		/* OnesOp */
		printf("Ones");
		break;
	case 0x06:		/* AliasOp */
		asl_dump_oparg(&dp, indent, "Alias", "tN");
		break;
	case 0x08:		/* NameOp */
		asl_dump_oparg(&dp, indent, "Name", "Nt");
		break;
	case 0x10:		/* ScopeOp */
		asl_dump_defscope(&dp, indent);
		break;
	case 0x11:		/* BufferOp */
		asl_dump_defbuffer(&dp, indent);
		break;
	case 0x12:		/* PackageOp */
		asl_dump_defpackage(&dp, indent);
		break;
	case 0x14:		/* MethodOp */
		asl_dump_defmethod(&dp, indent);
		break;
	case 0x5b:		/* ExtOpPrefix */
		opcode = *dp++;
		switch (opcode) {
		case 0x01:	/* MutexOp */
			printf("Mutex(");
			ASL_CREATE_LOCALNAMEOBJ(dp);
			asl_dump_termobj(&dp, indent);
			printf(", %d)", *dp++);
			break;
		case 0x02:	/* EventOp */
			asl_dump_oparg(&dp, indent, "Event", "t");
			break;
		case 0x12:	/* CondRefOfOp */
			asl_dump_oparg(&dp, indent, "CondRefOf", "tt");
			break;
		case 0x13:	/* CreateFieldOp */
			asl_dump_oparg(&dp, indent, "CreateField", "tiiN");
			break;
		case 0x1F:      /* LoadTableOp */
			asl_dump_oparg(&dp, indent, "LoadTable", "tttttt");
			break;
		case 0x20:	/* LoadOp */
			asl_dump_oparg(&dp, indent, "Load", "tt");
			break;
		case 0x21:	/* StallOp */
			asl_dump_oparg(&dp, indent, "Stall", "i");
			break;
		case 0x22:	/* SleepOp */
			asl_dump_oparg(&dp, indent, "Sleep", "i");
			break;
		case 0x23:	/* AcquireOp */
			asl_dump_oparg(&dp, indent, "Acquire", "tw");
			break;
		case 0x24:	/* SignalOp */
			asl_dump_oparg(&dp, indent, "Signal", "t");
			break;
		case 0x25:	/* WaitOp */
			asl_dump_oparg(&dp, indent, "Wait", "ti");
			break;
		case 0x26:	/* ResetOp */
			asl_dump_oparg(&dp, indent, "Reset", "t");
			break;
		case 0x27:	/* ReleaseOp */
			asl_dump_oparg(&dp, indent, "Release", "t");
			break;
		case 0x28:	/* FromBCDOp */
			asl_dump_oparg(&dp, indent, "FromBCD", "io");
			break;
		case 0x29:	/* ToBCDOp */
			asl_dump_oparg(&dp, indent, "ToBCD", "io");
			break;
		case 0x2a:	/* UnloadOp */
			asl_dump_oparg(&dp, indent, "Unload", "t");
			break;
		case 0x30:
			printf("Revision");
			break;
		case 0x31:
			printf("Debug");
			break;
		case 0x32:	/* FatalOp */
			asl_dump_oparg(&dp, indent, "Fatal", "bdi");
			break;
		case 0x33:      /* TimerOp */
			printf("Timer");
			break;
		case 0x80:	/* OpRegionOp */
			asl_dump_defopregion(&dp, indent);
			break;
		case 0x81:	/* FieldOp */
			asl_dump_deffield(&dp, indent);
			break;
		case 0x82:	/* DeviceOp */
			asl_dump_defdevice(&dp, indent);
			break;
		case 0x83:	/* ProcessorOp */
			asl_dump_defprocessor(&dp, indent);
			break;
		case 0x84:	/* PowerResOp */
			asl_dump_defpowerres(&dp, indent);
			break;
		case 0x85:	/* ThermalZoneOp */
			asl_dump_defthermalzone(&dp, indent);
			break;
		case 0x86:	/* IndexFieldOp */
			asl_dump_defindexfield(&dp, indent);
			break;
		case 0x87:	/* BankFieldOp */
			asl_dump_defbankfield(&dp, indent);
			break;
		case 0x88:      /* DataRegionOp */
			asl_dump_oparg(&dp, indent, "DataRegion", "Nttt");
			break;
		default:
			errx(1, "strange opcode 0x5b, 0x%x", opcode);
		}
		break;
	case 0x68:
	case 0x69:
	case 0x6a:
	case 0x6b:
	case 0x6c:
	case 0x6d:
	case 0x6e:	/* ArgN */
		printf("Arg%d", opcode - 0x68);
		break;
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:
		printf("Local%d", opcode - 0x60);
		break;
	case 0x70:		/* StoreOp */
		asl_dump_oparg(&dp, indent, "Store", "tt");
		break;
	case 0x71:		/* RefOfOp */
		asl_dump_oparg(&dp, indent, "RefOf", "t");
		break;
	case 0x72:		/* AddOp */
		asl_dump_oparg(&dp, indent, "Add", "iio");
		break;
	case 0x73:		/* ConcatenateOp */
		asl_dump_oparg(&dp, indent, "Concatenate", "ttt");
		break;
	case 0x74:		/* SubtractOp */
		asl_dump_oparg(&dp, indent, "Subtract", "iio");
		break;
	case 0x75:		/* IncrementOp */
		asl_dump_oparg(&dp, indent, "Increment", "t");
		break;
	case 0x76:		/* DecrementOp */
		asl_dump_oparg(&dp, indent, "Decrement", "t");
		break;
	case 0x77:		/* MultiplyOp */
		asl_dump_oparg(&dp, indent, "Multiply", "iio");
		break;
	case 0x78:		/* DivideOp */
		asl_dump_oparg(&dp, indent, "Divide", "iioo");
		break;
	case 0x79:		/* ShiftLeftOp */
		asl_dump_oparg(&dp, indent, "ShiftLeft", "iio");
		break;
	case 0x7a:		/* ShiftRightOp */
		asl_dump_oparg(&dp, indent, "ShiftRight", "iio");
		break;
	case 0x7b:		/* AndOp */
		asl_dump_oparg(&dp, indent, "And", "iio");
		break;
	case 0x7c:		/* NAndOp */
		asl_dump_oparg(&dp, indent, "NAnd", "iio");
		break;
	case 0x7d:		/* OrOp */
		asl_dump_oparg(&dp, indent, "Or", "iio");
		break;
	case 0x7e:		/* NOrOp */
		asl_dump_oparg(&dp, indent, "NOr", "iio");
		break;
	case 0x7f:		/* XOrOp */
		asl_dump_oparg(&dp, indent, "XOr", "iio");
		break;
	case 0x80:		/* NotOp */
		asl_dump_oparg(&dp, indent, "Not", "io");
		break;
	case 0x81:		/* FindSetLeftBitOp */
		asl_dump_oparg(&dp, indent, "FindSetLeftBit", "it");
		break;
	case 0x82:		/* FindSetRightBitOp */
		asl_dump_oparg(&dp, indent, "FindSetRightBit", "it");
		break;
	case 0x83:		/* DerefOp */
		asl_dump_oparg(&dp, indent, "DerefOf", "t");
		break;
	case 0x84:              /* ConcatResTemplateOp */
		asl_dump_oparg(&dp, indent, "ConcatResTemplate", "ttt");
		break;
	case 0x85:              /* ModOp */
		asl_dump_oparg(&dp, indent, "Mod", "iio");
		break;
	case 0x86:		/* NotifyOp */
		asl_dump_oparg(&dp, indent, "Notify", "tt");
		break;
	case 0x87:		/* SizeOfOp */
		asl_dump_oparg(&dp, indent, "SizeOf", "t");
		break;
	case 0x88:		/* IndexOp */
		asl_dump_oparg(&dp, indent, "Index", "tio");
		break;
	case 0x89:		/* MatchOp */
		printf("Match(");
		asl_dump_termobj(&dp, indent);
		printf(", %s, ", matchstr[*dp++]);
		asl_dump_termobj(&dp, indent);
		printf(", %s, ", matchstr[*dp++]);
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x8a:		/* CreateDWordFieldOp */
		asl_dump_oparg(&dp, indent, "CreateDWordField", "tiN");
		break;
	case 0x8b:		/* CreateWordFieldOp */
		asl_dump_oparg(&dp, indent, "CreateWordField", "tiN");
		break;
	case 0x8c:		/* CreateByteFieldOp */
		asl_dump_oparg(&dp, indent, "CreateByteField", "tiN");
		break;
	case 0x8d:		/* CreateBitFieldOp */
		asl_dump_oparg(&dp, indent, "CreateBitField", "tiN");
		break;
	case 0x8e:		/* ObjectTypeOp */
		asl_dump_oparg(&dp, indent, "ObjectType", "t");
		break;
	case 0x8f:		/* CreateQWordFieldOp */
		asl_dump_oparg(&dp, indent, "CreateQWordField", "tiN");
		break;
	case 0x90:
		asl_dump_oparg(&dp, indent, "LAnd", "ii");
		break;
	case 0x91:
		asl_dump_oparg(&dp, indent, "LOr", "ii");
		break;
	case 0x92:
		asl_dump_oparg(&dp, indent, "LNot", "i");
		break;
	case 0x93:
		asl_dump_oparg(&dp, indent, "LEqual", "tt");
		break;
	case 0x94:
		asl_dump_oparg(&dp, indent, "LGreater", "tt");
		break;
	case 0x95:
		asl_dump_oparg(&dp, indent, "LLess", "tt");
		break;
	case 0x96:		/* ToBufferOp */
		asl_dump_oparg(&dp, indent, "ToBuffer", "to");
		break;
	case 0x97:		/* ToDecStringOp */
		asl_dump_oparg(&dp, indent, "ToDecString", "to");
		break;
	case 0x98:		/* ToHexStringOp */
		asl_dump_oparg(&dp, indent, "ToHexString", "to");
		break;
	case 0x99:		/* ToIntegerOp */
		asl_dump_oparg(&dp, indent, "ToInteger", "to");
		break;
	case 0x9c:		/* ToStringOp */
		asl_dump_oparg(&dp, indent, "ToString", "tto");
		break;
	case 0x9d:               /* CopyObjectOp */
		asl_dump_oparg(&dp, indent, "CopyObject", "tt");
		break;
	case 0x9e:              /* MidOp */
		asl_dump_oparg(&dp, indent, "Mid", "tiio");
		break;
	case 0x9f:
		printf("Continue");
		break;
	case 0xa0:		/* IfOp */
		asl_dump_defif(&dp, indent);
		break;
	case 0xa1:		/* ElseOp */
		asl_dump_defelse(&dp, indent);
		break;
	case 0xa2:		/* WhileOp */
		asl_dump_defwhile(&dp, indent);
		break;
	case 0xa3:		/* NoopOp */
		printf("Noop");
		break;
	case 0xa5:		/* BreakOp */
		printf("Break");
		break;
	case 0xa4:		/* ReturnOp */
		asl_dump_oparg(&dp, indent, "Return", "t");
		break;
	case 0xcc:		/* BreakPointOp */
		printf("BreakPoint");
		break;
	default:
		errx(1, "strange opcode 0x%x", opcode);
	}

	*dpp = dp;
}

void
asl_dump_objectlist(u_int8_t **dpp, u_int8_t *end, int indent)
{
	u_int8_t	*dp;

	dp = *dpp;
	while (dp < end) {
		print_indent(indent);
		asl_dump_termobj(&dp, indent);
		printf("\n");
	}

	*dpp = dp;
}
