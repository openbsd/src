/* Generated from /home/biorn/src/lib/libkrb5/../../kerberosV/src/lib/hdb/hdb_err.et */
/* $KTH: hdb_err.et,v 1.5 2001/01/28 23:05:52 assar Exp $ */

#include <stddef.h>
#include <com_err.h>
#include "hdb_err.h"

static const char *hdb_error_strings[] = {
	/* 000 */ "Reserved hdb error (0)",
	/* 001 */ "Database store error",
	/* 002 */ "Database read error",
	/* 003 */ "No such entry in the database",
	/* 004 */ "Database is locked or in use--try again later",
	/* 005 */ "Database was modified during read",
	/* 006 */ "Attempt to lock database twice",
	/* 007 */ "Attempt to unlock database when not locked",
	/* 008 */ "Invalid kdb lock mode",
	/* 009 */ "Insufficient access to lock database",
	/* 010 */ "Entry already exists in database",
	/* 011 */ "Wrong database version",
	/* 012 */ "No correct master key",
	NULL
};

#define num_errors 13

void initialize_hdb_error_table_r(struct et_list **list)
{
    initialize_error_table_r(list, hdb_error_strings, num_errors, ERROR_TABLE_BASE_hdb);
}

void initialize_hdb_error_table(void)
{
    init_error_table(hdb_error_strings, ERROR_TABLE_BASE_hdb, num_errors);
}
