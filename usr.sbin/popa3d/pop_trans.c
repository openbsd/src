/* $OpenBSD: pop_trans.c,v 1.3 2002/09/06 19:17:52 deraadt Exp $ */

/*
 * TRANSACTION state handling.
 */

#include <sys/types.h>
#include <stdio.h>
#include <syslog.h>

#include "params.h"
#include "protocol.h"
#include "database.h"
#include "mailbox.h"

static int pop_trans_quit(char *params)
{
	if (params) return POP_ERROR;
	return POP_STATE;
}

static int pop_trans_noop(char *params)
{
	if (params) return POP_ERROR;
	return POP_OK;
}

static int pop_trans_stat(char *params)
{
	if (params) return POP_ERROR;
	if (pop_reply("+OK %d %ld", db.visible_count, db.visible_size))
		return POP_CRASH_NETFAIL;
	return POP_QUIET;
}

static int pop_trans_list_or_uidl(char *params, int uidl)
{
	int number;
	struct db_message *msg;

	if (params) {
		number = pop_get_int(&params);
		if (number < 1 || number > db.total_count || params)
			return POP_ERROR;
		msg = db.array[number - 1];
		if (msg->flags & MSG_DELETED) return POP_ERROR;
		if (uidl) {
			if (pop_reply("+OK %d "
			    "%02x%02x%02x%02x%02x%02x%02x%02x",
			    number,
			    msg->hash[3], msg->hash[2],
			    msg->hash[1], msg->hash[0],
			    msg->hash[7], msg->hash[6],
			    msg->hash[5], msg->hash[4]))
				return POP_CRASH_NETFAIL;
		} else
			if (pop_reply("+OK %d %ld", number, msg->size))
				return POP_CRASH_NETFAIL;
		return POP_QUIET;
	}

	if (pop_reply_ok()) return POP_CRASH_NETFAIL;
	for (number = 1; number <= db.total_count; number++) {
		msg = db.array[number - 1];
		if (msg->flags & MSG_DELETED) continue;
		if (uidl) {
			if (pop_reply("%d "
			    "%02x%02x%02x%02x%02x%02x%02x%02x",
			    number,
			    msg->hash[3], msg->hash[2],
			    msg->hash[1], msg->hash[0],
			    msg->hash[7], msg->hash[6],
			    msg->hash[5], msg->hash[4]))
				return POP_CRASH_NETFAIL;
		} else
			if (pop_reply("%d %ld", number, msg->size))
				return POP_CRASH_NETFAIL;
	}
	if (pop_reply_terminate()) return POP_CRASH_NETFAIL;

	return POP_QUIET;
}

static int pop_trans_list(char *params)
{
	return pop_trans_list_or_uidl(params, 0);
}

static int pop_trans_uidl(char *params)
{
	return pop_trans_list_or_uidl(params, 1);
}

static int pop_trans_retr(char *params)
{
	int number;
	struct db_message *msg;
	int event;

	number = pop_get_int(&params);
	if (number < 1 || number > db.total_count || params) return POP_ERROR;
	msg = db.array[number - 1];
	if (msg->flags & MSG_DELETED) return POP_ERROR;
	if ((event = mailbox_get(msg, -1)) != POP_OK) return event;
#if POP_SUPPORT_LAST
	if (number > db.last) db.last = number;
#endif
	return POP_QUIET;
}

static int pop_trans_top(char *params)
{
	int number, lines;
	struct db_message *msg;
	int event;

	number = pop_get_int(&params);
	if (number < 1 || number > db.total_count) return POP_ERROR;
	lines = pop_get_int(&params);
	if (lines < 0 || params) return POP_ERROR;
	msg = db.array[number - 1];
	if (msg->flags & MSG_DELETED) return POP_ERROR;
	if ((event = mailbox_get(msg, lines)) != POP_OK) return event;
	return POP_QUIET;
}

static int pop_trans_dele(char *params)
{
	int number;
	struct db_message *msg;

	number = pop_get_int(&params);
	if (number < 1 || number > db.total_count || params) return POP_ERROR;
	msg = db.array[number - 1];
	if (db_delete(msg)) return POP_ERROR;
#if POP_SUPPORT_LAST
	if (number > db.last) db.last = number;
#endif
	return POP_OK;
}

static int pop_trans_rset(char *params)
{
	struct db_message *msg;

	if (params) return POP_ERROR;

	if ((msg = db.head))
	do {
		msg->flags &= ~MSG_DELETED;
	} while ((msg = msg->next));

	db.visible_count = db.total_count;
	db.visible_size = db.total_size;
	db.flags &= ~DB_DIRTY;
#if POP_SUPPORT_LAST
	db.last = 0;
#endif

	return POP_OK;
}

#if POP_SUPPORT_LAST
static int pop_trans_last(char *params)
{
	if (params) return POP_ERROR;
	if (pop_reply("+OK %d", db.last)) return POP_CRASH_NETFAIL;
	return POP_QUIET;
}
#endif

static struct pop_command pop_trans_commands[] = {
	{"QUIT", pop_trans_quit},
	{"NOOP", pop_trans_noop},
	{"STAT", pop_trans_stat},
	{"LIST", pop_trans_list},
	{"UIDL", pop_trans_uidl},
	{"RETR", pop_trans_retr},
	{"TOP", pop_trans_top},
	{"DELE", pop_trans_dele},
	{"RSET", pop_trans_rset},
#if POP_SUPPORT_LAST
	{"LAST", pop_trans_last},
#endif
	{NULL}
};

static int db_load(char *spool, char *mailbox)
{
	db_init();

	if (mailbox_open(spool, mailbox)) return 1;

	if (db_fix()) {
		mailbox_close();
		return 1;
	}

	return 0;
}

int do_pop_trans(char *spool, char *mailbox)
{
	int result;

	if (!pop_sane()) return 1;

	if (db_load(spool, mailbox)) {
		syslog(SYSLOG_PRI_HI,
			"Failed or refused to load %s/%s",
			spool, mailbox);
		pop_reply_error();
		return 0;
	}

	syslog(SYSLOG_PRI_LO, "%d message%s (%ld byte%s) loaded",
		db.total_count, db.total_count == 1 ? "" : "s",
		db.total_size, db.total_size == 1 ? "" : "s");

	if (pop_reply_ok())
		result = POP_CRASH_NETFAIL;
	else
	switch ((result = pop_handle_state(pop_trans_commands))) {
	case POP_STATE:
		if (mailbox_update()) {
			if (db.flags & DB_STALE) break;
			syslog(SYSLOG_PRI_ERROR,
				"Failed to update %s/%s",
				spool, mailbox);
			pop_reply_error();
			break;
		}

		syslog(SYSLOG_PRI_LO, "%d (%ld) deleted, %d (%ld) left",
			db.total_count - db.visible_count,
			db.total_size - db.visible_size,
			db.visible_count,
			db.visible_size);
		pop_reply_ok();
		break;

	case POP_CRASH_NETFAIL:
		syslog(SYSLOG_PRI_LO, "Premature disconnect");
		break;

	case POP_CRASH_NETTIME:
		syslog(SYSLOG_PRI_LO, "Connection timed out");
	}

	if (db.flags & DB_STALE)
		syslog(SYSLOG_PRI_LO, "Another MUA active, giving up");
	else
	if (result == POP_CRASH_SERVER)
		syslog(SYSLOG_PRI_ERROR,
			"Server failure accessing %s/%s",
			spool, mailbox);

	mailbox_close();

	return 0;
}
