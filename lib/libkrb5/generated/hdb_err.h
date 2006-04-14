/* Generated from /home/biorn/src/lib/libkrb5/../../kerberosV/src/lib/hdb/hdb_err.et */
/* $KTH: hdb_err.et,v 1.5 2001/01/28 23:05:52 assar Exp $ */

#ifndef __hdb_err_h__
#define __hdb_err_h__

struct et_list;

void initialize_hdb_error_table_r(struct et_list **);

void initialize_hdb_error_table(void);
#define init_hdb_err_tbl initialize_hdb_error_table

typedef enum hdb_error_number{
	HDB_ERR_UK_SERROR = 36150273,
	HDB_ERR_UK_RERROR = 36150274,
	HDB_ERR_NOENTRY = 36150275,
	HDB_ERR_DB_INUSE = 36150276,
	HDB_ERR_DB_CHANGED = 36150277,
	HDB_ERR_RECURSIVELOCK = 36150278,
	HDB_ERR_NOTLOCKED = 36150279,
	HDB_ERR_BADLOCKMODE = 36150280,
	HDB_ERR_CANT_LOCK_DB = 36150281,
	HDB_ERR_EXISTS = 36150282,
	HDB_ERR_BADVERSION = 36150283,
	HDB_ERR_NO_MKEY = 36150284
} hdb_error_number;

#define ERROR_TABLE_BASE_hdb 36150272

#endif /* __hdb_err_h__ */
