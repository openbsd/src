/* $OpenBSD: database.h,v 1.2 2003/05/12 19:28:22 camield Exp $ */

/*
 * Message database management.
 */

#ifndef _POP_DATABASE_H
#define _POP_DATABASE_H

#include "params.h"

/*
 * Message flags.
 */
/* Marked for deletion */
#define MSG_DELETED			0x00000001

/*
 * Database flags.
 */
/* Some messages are marked for deletion, mailbox update is needed */
#define DB_DIRTY			0x00000001
/* Another MUA has modified our part of the mailbox */
#define DB_STALE			0x00000002

struct db_message {
	struct db_message *next;
	unsigned long size;		/* Size as reported via POP */
	unsigned int flags;		/* MSG_* flags defined above */
	unsigned long raw_offset;	/* Raw, with the "From " line */
	unsigned long raw_size;
	unsigned long data_offset;	/* Just the message itself */
	unsigned long data_size;
	unsigned char hash[16];		/* MD5 hash, to be used for UIDL */
};

struct db_main {
	struct db_message *head, *tail;	/* Messages in a linked list */
	struct db_message **array;	/* Direct access to messages */
	unsigned int total_count;	/* All loaded messages and */
	unsigned int visible_count;	/* just those not DELEted */
	unsigned long total_size;	/* Their cumulative sizes, */
	unsigned long visible_size;	/* to be reported via POP */
	unsigned int flags;		/* DB_* flags defined above */
#if POP_SUPPORT_LAST
	unsigned int last;		/* Last message touched */
#endif
};

extern struct db_main db;

extern void db_init(void);
extern int db_add(struct db_message *msg);
extern int db_delete(struct db_message *msg);
extern int db_fix(void);

#endif
