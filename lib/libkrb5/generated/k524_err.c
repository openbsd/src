/* Generated from /home/biorn/src/lib/libkrb5/../../kerberosV/src/lib/krb5/k524_err.et */
/* $KTH: k524_err.et,v 1.1 2001/06/20 02:44:11 joda Exp $ */

#include <stddef.h>
#include <com_err.h>
#include "k524_err.h"

static const char *k524_error_strings[] = {
	/* 000 */ "wrong keytype in ticket",
	/* 001 */ "incorrect network address",
	/* 002 */ "cannot convert V5 principal",
	/* 003 */ "V5 realm name longer than V4 maximum",
	/* 004 */ "kerberos V4 error server",
	/* 005 */ "encoding too large at server",
	/* 006 */ "decoding out of data",
	/* 007 */ "service not responding",
	NULL
};

#define num_errors 8

void initialize_k524_error_table_r(struct et_list **list)
{
    initialize_error_table_r(list, k524_error_strings, num_errors, ERROR_TABLE_BASE_k524);
}

void initialize_k524_error_table(void)
{
    init_error_table(k524_error_strings, ERROR_TABLE_BASE_k524, num_errors);
}
