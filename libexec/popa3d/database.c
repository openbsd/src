/* $OpenBSD: database.c,v 1.2 2001/08/13 20:19:33 camield Exp $ */

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

	if (db.total_count >= MAX_MAILBOX_MESSAGES) return 1;

	entry = malloc(sizeof(struct db_message));
	if (!entry) return 1;

	memcpy(entry, msg, sizeof(struct db_message));
	entry->next = NULL;
	entry->flags = 0;

	if (db.tail)
		db.tail = db.tail->next = entry;
	else
		db.tail = db.head = entry;

	if (++db.total_count <= 0) return 1;
	if ((db.total_size += entry->size) < 0 || entry->size < 0) return 1;

	return 0;
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
	int size;
	struct db_message *entry;
	int index;

	db.visible_count = db.total_count;
	db.visible_size = db.total_size;

	if (!db.total_count) return 0;

	size = sizeof(struct db_message *) * db.total_count;
	if (size <= 0) return 1;
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
