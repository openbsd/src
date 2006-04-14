/* Generated from /home/biorn/src/lib/libkrb5/../../kerberosV/src/lib/asn1/asn1_err.et */
/* $KTH: asn1_err.et,v 1.5 1998/02/16 16:17:17 joda Exp $ */

#ifndef __asn1_err_h__
#define __asn1_err_h__

struct et_list;

void initialize_asn1_error_table_r(struct et_list **);

void initialize_asn1_error_table(void);
#define init_asn1_err_tbl initialize_asn1_error_table

typedef enum asn1_error_number{
	ASN1_BAD_TIMEFORMAT = 1859794432,
	ASN1_MISSING_FIELD = 1859794433,
	ASN1_MISPLACED_FIELD = 1859794434,
	ASN1_TYPE_MISMATCH = 1859794435,
	ASN1_OVERFLOW = 1859794436,
	ASN1_OVERRUN = 1859794437,
	ASN1_BAD_ID = 1859794438,
	ASN1_BAD_LENGTH = 1859794439,
	ASN1_BAD_FORMAT = 1859794440,
	ASN1_PARSE_ERROR = 1859794441
} asn1_error_number;

#define ERROR_TABLE_BASE_asn1 1859794432

#endif /* __asn1_err_h__ */
