/* $OpenBSD: database.h,v 1.2 2001/08/13 20:19:33 camield Exp $ */

/*
 * Message database management.
 */

#ifndef _POP_DATABASE_H
#define _POP_DATABASE_H

#include <md5.h>

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
	long size;			/* Size as reported via POP */
	int flags;			/* MSG_* flags defined above */
	long raw_offset, raw_size;	/* Raw, with the "From " line */
	long data_offset, data_size;	/* Just the message itself */
	unsigned char hash[16];		/* MD5 hash, to be used for UIDL */
};

struct db_main {
	struct db_message *head, *tail;	/* Messages in a linked list */
	struct db_message **array;	/* Direct access to messages */
	int total_count, visible_count;	/* Total and not DELEted counts */
	long total_size, visible_size;	/* To be reported via POP */
	int flags;			/* DB_* flags defined above */
#if POP_SUPPORT_LAST
	int last;			/* Last message touched */
#endif
};

extern struct db_main db;

extern void db_init(void);
extern int db_add(struct db_message *msg);
extern int db_delete(struct db_message *msg);
extern int db_fix(void);

#endif
