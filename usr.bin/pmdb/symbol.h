/*	$OpenBSD: symbol.h,v 1.4 2002/03/15 18:04:41 art Exp $	*/
/*
 * Copyright (c) 2002 Artur Grabowski <art@openbsd.org>
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

struct sym_table {
	TAILQ_ENTRY(sym_table) st_list;
	char st_fname[MAXPATHLEN];
	int st_flags;
	reg st_offs;
};

/* Flags in st_flags */
#define ST_EXEC		0x01	/* this is the executable */

struct sym_ops {
	struct sym_table *(*sop_open)(const char *);
	void (*sop_close)(struct sym_table *);
	char *(*sop_name_and_off)(struct sym_table *, reg, reg *);
	int (*sop_lookup)(struct pstate *, const char *, reg *);
	void (*sop_update)(struct pstate *);
};

void sym_init_exec(struct pstate *, const char *);
void sym_destroy(struct pstate *);
void sym_update(struct pstate *);
char *sym_name_and_offset(struct pstate *, reg, char *, size_t, reg *);
int sym_lookup(struct pstate *, const char *, reg *);
char *sym_print(struct pstate *, reg, char *, size_t);

/* Internal for symbol handlers only. */
struct sym_table *st_open(struct pstate *, const char *, reg);

#ifdef PMDB_ELF
int sym_check_elf(const char *, struct pstate *);
#endif
#ifdef PMDB_AOUT
int sym_check_aout(const char *, struct pstate *);
#endif

int cmd_sym_load(int, char **, void *);
