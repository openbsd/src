/* $OpenBSD: database.c,v 1.2 2003/05/12 19:28:22 camield Exp $ */

/*
 * Message database management.
 */

#include <stdlib.h>
#include <string.h>

#include "params.h"
#include "database.h"

struct db_main db;

void db_init(void)
{
	db.head = db.tail = NULL;
	db.total_count = 0;
	db.total_size = 0;
	db.flags = 0;
#if POP_SUPPORT_LAST
	db.last = 0;
#endif
}

int db_add(struct db_message *msg)
{
	struct db_message *entry;

	if (db.total_count >= MAX_MAILBOX_MESSAGES) goto out_fail;
	if (++db.total_count <= 0) goto out_undo_count;
	if ((db.total_size += msg->size) < msg->size) goto out_undo_size;

	entry = malloc(sizeof(struct db_message));
	if (!entry) goto out_undo_size;

	memcpy(entry, msg, sizeof(struct db_message));
	entry->next = NULL;
	entry->flags = 0;

	if (db.tail)
		db.tail = db.tail->next = entry;
	else
		db.tail = db.head = entry;

	return 0;

out_undo_size:
	db.total_size -= msg->size;

out_undo_count:
	db.total_count--;

out_fail:
	return 1;
}

int db_delete(struct db_message *msg)
{
	if (msg->flags & MSG_DELETED) return 1;

	msg->flags |= MSG_DELETED;

	db.visible_count--;
	db.visible_size -= msg->size;
	db.flags |= DB_DIRTY;

	return 0;
}

int db_fix(void)
{
	unsigned long size;
	struct db_message *entry;
	unsigned int index;

	db.visible_count = db.total_count;
	db.visible_size = db.total_size;

	if (!db.total_count) return 0;

	size = sizeof(struct db_message *) * db.total_count;
	if (size / sizeof(struct db_message *) != db.total_count) return 1;

	db.array = malloc(size);
	if (!db.array) return 1;

	entry = db.head;
	index = 0;
	do {
		db.array[index++] = entry;
	} while ((entry = entry->next));

	return 0;
}
