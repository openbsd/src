/* $OpenBSD: db_machdep.h,v 1.1.1.1 2006/04/18 10:56:58 miod Exp $ */
/* public domain */
#include <m88k/db_machdep.h>

#ifdef DDB

void m88k_db_prom_cmd(db_expr_t, int, db_expr_t, char *);

#define	EXTRA_MACHDEP_COMMANDS \
	{ "prom",	m88k_db_prom_cmd,		0,	NULL },

#endif
