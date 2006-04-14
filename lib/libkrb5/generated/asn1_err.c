/* Generated from /home/biorn/src/lib/libkrb5/../../kerberosV/src/lib/asn1/asn1_err.et */
/* $KTH: asn1_err.et,v 1.5 1998/02/16 16:17:17 joda Exp $ */

#include <stddef.h>
#include <com_err.h>
#include "asn1_err.h"

static const char *asn1_error_strings[] = {
	/* 000 */ "ASN.1 failed call to system time library",
	/* 001 */ "ASN.1 structure is missing a required field",
	/* 002 */ "ASN.1 unexpected field number",
	/* 003 */ "ASN.1 type numbers are inconsistent",
	/* 004 */ "ASN.1 value too large",
	/* 005 */ "ASN.1 encoding ended unexpectedly",
	/* 006 */ "ASN.1 identifier doesn't match expected value",
	/* 007 */ "ASN.1 length doesn't match expected value",
	/* 008 */ "ASN.1 badly-formatted encoding",
	/* 009 */ "ASN.1 parse error",
	NULL
};

#define num_errors 10

void initialize_asn1_error_table_r(struct et_list **list)
{
    initialize_error_table_r(list, asn1_error_strings, num_errors, ERROR_TABLE_BASE_asn1);
}

void initialize_asn1_error_table(void)
{
    init_error_table(asn1_error_strings, ERROR_TABLE_BASE_asn1, num_errors);
}
