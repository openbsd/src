/* $OpenBSD: mailbox.c,v 1.4 2002/09/06 19:18:10 deraadt Exp $ */

/*
 * Mailbox access.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

#include <md5.h>

#include "misc.h"
#include "params.h"
#include "protocol.h"
#include "database.h"

static int mailbox_fd;			/* fd for the mailbox, or -1 */
static time_t mailbox_mtime;		/* mtime, as of the last check */
static long mailbox_size;		/* Its original size */

static struct db_message *cmp;

/*
 * If a message has changed since the database was filled in, then we
 * consider the database stale. This is called for every message when
 * the mailbox is being re-parsed (because of its mtime change).
 */
static int db_compare(struct db_message *msg)
{
	if (!cmp) return 1;

	if (msg->raw_size != cmp->raw_size || msg->size != cmp->size ||
	    memcmp(msg->hash, cmp->hash, sizeof(msg->hash))) {
		db.flags |= DB_STALE;
		return 1;
	}

	cmp = cmp->next;

	return 0;
}

/*
 * Checks if the buffer pointed to by s1, of n1 chars, starts with the
 * string s2, of n2 chars.
 */
#ifdef __GNUC__
__inline
#endif
static int linecmp(char *s1, int n1, char *s2, int n2)
{
	if (s1[0] != s2[0] || n1 < n2) return 1;
	return memcmp(s1, s2, n2);
}

/*
 * The mailbox parsing routine: first called to fill the database in,
 * then to check if the database is still up to date. We implement a
 * state machine at the line fragment level (that is, full or partial
 * lines). This is faster than dealing with individual characters (we
 * leave that job for libc), and doesn't require ever loading entire
 * lines into memory.
 */
static int mailbox_parse(int init)
{
	struct stat stat;			/* File information */
	struct db_message msg;			/* Message being parsed */
	MD5_CTX hash;				/* Its hash being computed */
	int (*db_op)(struct db_message *msg);	/* db_add or db_compare */
	char *file_buffer, *line_buffer;	/* Our internal buffers */
	long file_offset, line_offset, offset;	/* Their offsets in the file */
	char *current, *next, *line;		/* Line pointers */
	int block, saved, extra, length;	/* Internal block sizes */
	int done, start, end;			/* Various boolean flags: */
	int blank, header, body, fixed;		/* the state information */

	if (fstat(mailbox_fd, &stat)) return 1;

	if (init) {
/* Prepare for the database initialization */
		if (!S_ISREG(stat.st_mode)) return 1;
		mailbox_mtime = stat.st_mtime;
		mailbox_size = stat.st_size;
		if (mailbox_size < 0) return 1;
		if (!mailbox_size) return 0;
		if (mailbox_size > MAX_MAILBOX_BYTES) return 1;
		db_op = db_add;
	} else {
/* Prepare for checking against the database */
		if (mailbox_mtime == stat.st_mtime) return 0;
		if (!mailbox_size) return 0;
		if (mailbox_size > (long)stat.st_size) {
			db.flags |= DB_STALE;
			return 1;
		}
		if (lseek(mailbox_fd, 0, SEEK_SET) < 0) return 1;
		db_op = db_compare; cmp = db.head;
	}

	memset(&msg, 0, sizeof(msg));
	MD5Init(&hash);

	file_buffer = malloc(FILE_BUFFER_SIZE + LINE_BUFFER_SIZE);
	if (!file_buffer) return 1;
	line_buffer = &file_buffer[FILE_BUFFER_SIZE];

	file_offset = 0; line_offset = 0; offset = 0;	/* Start at 0, with */
	current = file_buffer; block = 0; saved = 0;	/* empty buffers */

	done = 0;	/* Haven't reached EOF or the original size yet */
	end = 1;	/* Assume we've just seen a LF: parse a new line */
	blank = 1;	/* Assume we've seen a blank line: look for "From " */
	header = 0;	/* Not in message headers, */
	body = 0;	/* and not in message body */
	fixed = 0;	/* Not in a "fixed" part of a message */

/*
 * The main loop. Its first part extracts the line fragments, while the
 * second one manages the state flags and performs whatever is required
 * based on the state. Unfortunately, splitting this into two functions
 * didn't seem to simplify the code.
 */
	do {
/*
 * Part 1.
 * The line fragment extraction.
 */

/* Look for the next LF in the file buffer */
		if ((next = memchr(current, '\n', block))) {
/* Found it: get the length of this piece, and check for buffered data */
			length = ++next - current;
			if (saved) {
/* Have this line's beginning in the line buffer: combine them */
				extra = LINE_BUFFER_SIZE - saved;
				if (extra > length) extra = length;
				memcpy(&line_buffer[saved], current, extra);
				current += extra; block -= extra;
				length = saved + extra;
				line = line_buffer;
				offset = line_offset;
				start = end; end = current == next;
				saved = 0;
			} else {
/* Nothing in the line buffer: just process what we've got now */
				line = current;
				offset = file_offset - block;
				start = end; end = 1;
				current = next; block -= length;
			}
		} else {
/* No more LF's in the file buffer */
			if (saved || block <= LINE_BUFFER_SIZE) {
/* Have this line's beginning in the line buffer: combine them */
/* Not enough data to process right now: buffer it */
				extra = LINE_BUFFER_SIZE - saved;
				if (extra > block) extra = block;
				if (!saved) line_offset = file_offset - block;
				memcpy(&line_buffer[saved], current, extra);
				current += extra; block -= extra;
				saved += extra;
				length = saved;
				line = line_buffer;
				offset = line_offset;
			} else {
/* Nothing in the line buffer and we've got enough data: just process it */
				length = block - 1;
				line = current;
				offset = file_offset - block;
				current += length;
				block = 1;
			}
			if (!block) {
/* We've emptied the file buffer: fetch some more data */
				current = file_buffer;
				if (init)
					block = FILE_BUFFER_SIZE;
				else {
					block = mailbox_size - file_offset;
					if (block > FILE_BUFFER_SIZE)
						block = FILE_BUFFER_SIZE;
				}
				block = read(mailbox_fd, file_buffer, block);
				if (block < 0) break;
				file_offset += block;
				if (block > 0 && saved < LINE_BUFFER_SIZE)
					continue;
				if (!saved) {
/* Nothing in the line buffer, and read(2) returned 0: we're done */
					offset = file_offset;
					done = 1;
					break;
				}
			}
			start = end; end = !block;
			saved = 0;
		}

/*
 * Part 2.
 * The following variables are set when we get here:
 * -- line	the line fragment, not NUL terminated;
 * -- length	its length;
 * -- offset	its offset in the file;
 * -- start	whether it's at the start of the line;
 * -- end	whether it's at the end of the line
 * (all four combinations of "start" and "end" are possible).
 */

/* Check for a new message if we've just seen a blank line */
		if (blank && start)
		if (!linecmp(line, length, "From ", 5)) {
/* Process the previous one first, if exists */
			if (offset) {
				if (!header && !body) break;
				msg.raw_size = offset - msg.raw_offset;
				msg.data_size = offset - 1 - msg.data_offset;
				MD5Final(msg.hash, &hash);
				if (db_op(&msg)) break;
			}
/* Now prepare for parsing the new one */
			msg.raw_offset = offset;
			msg.data_offset = 0;
			MD5Init(&hash);
			header = 1; body = 0;
			continue;
		}

/* Memorize file offset of the message data (the line next to "From ") */
		if (header && start && !msg.data_offset) {
			msg.data_offset = offset;
			msg.data_size = 0;
			msg.size = 0;
		}

/* Count this fragment, with LF's as CRLF, into the message size */
		if (msg.data_offset)
			msg.size += length + end;

/* If we see LF at start of line, then this is a blank line :-) */
		blank = start && line[0] == '\n';

/* Blank line in headers means start of the message body */
		if (header && blank) {
			header = 0; body = 1;
		}

/* Some header lines are known to remain fixed over MUA runs */
		if (header && start)
		if (!linecmp(line, length, "Received:", 9) ||
		    !linecmp(line, length, "Date:", 5) ||
		    !linecmp(line, length, "Message-Id:", 11) ||
		    !linecmp(line, length, "Subject:", 8))
			fixed = 1;

/* We can hash all fragments of those lines (until "end"), for UIDL */
		if (fixed) {
			MD5Update(&hash, (u_char *)line, length);
			if (end) fixed = 0;
		}
	} while (1);

	free(file_buffer);

	if (done) {
/* Process the last message */
		if (offset != mailbox_size) return 1;
		if (!header && !body) return 1;
		msg.raw_size = offset - msg.raw_offset;
		msg.data_size = offset - blank - msg.data_offset;
		MD5Final(msg.hash, &hash);
		if (db_op(&msg)) return 1;

/* Everything went well, update our timestamp if we were checking */
		if (!init) mailbox_mtime = stat.st_mtime;
	}

	return !done;
}

int mailbox_open(char *spool, char *mailbox)
{
	char *pathname;
	struct stat stat;
	int result;

	mailbox_fd = -1;

	pathname = malloc(strlen(spool) + strlen(mailbox) + 2);
	if (!pathname) return 1;
	sprintf(pathname, "%s/%s", spool, mailbox);

	if (lstat(pathname, &stat)) {
		free(pathname);
		return errno != ENOENT;
	}

	if (!S_ISREG(stat.st_mode)) {
		free(pathname);
		return 1;
	}

	if (!stat.st_size) {
		free(pathname);
		return 0;
	}

	mailbox_fd = open(pathname, O_RDWR | O_NOCTTY | O_NONBLOCK);

	free(pathname);

	if (mailbox_fd < 0)
		return errno != ENOENT;

	if (lock_fd(mailbox_fd, 1)) return 1;

	result = mailbox_parse(1);

	if (!result && time(NULL) == mailbox_mtime)
	if (sleep_select(1, 0)) result = 1;

	if (unlock_fd(mailbox_fd)) return 1;

	return result;
}

static int mailbox_changed(void)
{
	struct stat stat;
	int result;

	if (fstat(mailbox_fd, &stat)) return 1;
	if (mailbox_mtime == stat.st_mtime) return 0;

	if (lock_fd(mailbox_fd, 1)) return 1;

	result = mailbox_parse(0);

	if (!result && time(NULL) == mailbox_mtime)
	if (sleep_select(1, 0)) result = 1;

	if (unlock_fd(mailbox_fd)) return 1;

	return result;
}

int mailbox_get(struct db_message *msg, int lines)
{
	if (mailbox_changed()) return POP_CRASH_SERVER;

	if (lseek(mailbox_fd, msg->data_offset, SEEK_SET) < 0)
		return POP_CRASH_SERVER;
	if (pop_reply_multiline(mailbox_fd, msg->data_size, lines))
		return POP_CRASH_NETFAIL;

	if (mailbox_changed()) return POP_CRASH_SERVER;

	if (pop_reply_terminate()) return POP_CRASH_NETFAIL;

	return POP_OK;
}

static int mailbox_write(char *buffer)
{
	struct db_message *msg;
	long old, new;
	int block;

	msg = db.head;
	old = new = 0;
	do {
		if (msg->flags & MSG_DELETED) continue;
		old = msg->raw_offset;

		if (old == new) {
			old = (new += msg->raw_size);
			continue;
		}

		while ((block = msg->raw_size - (old - msg->raw_offset))) {
			if (lseek(mailbox_fd, old, SEEK_SET) < 0) return 1;
			if (block > FILE_BUFFER_SIZE) block = FILE_BUFFER_SIZE;
			block = read(mailbox_fd, buffer, block);
			if (!block && old == mailbox_size) break;
			if (block <= 0) return 1;

			if (lseek(mailbox_fd, new, SEEK_SET) < 0) return 1;
			if (write_loop(mailbox_fd, buffer, block) != block)
				return 1;

			old += block; new += block;
		}
	} while ((msg = msg->next));

	old = mailbox_size;
	while (1) {
		if (lseek(mailbox_fd, old, SEEK_SET) < 0) return 1;
		block = read(mailbox_fd, buffer, FILE_BUFFER_SIZE);
		if (!block) break;
		if (block < 0) return 1;

		if (lseek(mailbox_fd, new, SEEK_SET) < 0) return 1;
		if (write(mailbox_fd, buffer, block) != block) return 1;

		old += block; new += block;
	}

	if (ftruncate(mailbox_fd, new)) return 1;

	return fsync(mailbox_fd);
}

static int mailbox_write_blocked(void)
{
	sigset_t blocked_set, old_set;
	char *buffer;
	int result;

	if (sigfillset(&blocked_set)) return 1;
	if (sigprocmask(SIG_BLOCK, &blocked_set, &old_set)) return 1;

	if ((buffer = malloc(FILE_BUFFER_SIZE))) {
		result = mailbox_write(buffer);
		free(buffer);
	} else
		result = 1;

	if (sigprocmask(SIG_SETMASK, &old_set, NULL)) return 1;

	return result;
}

int mailbox_update(void)
{
	int result;

	if (mailbox_fd < 0 || !(db.flags & DB_DIRTY)) return 0;

	if (lock_fd(mailbox_fd, 0)) return 1;

	if (!(result = mailbox_parse(0)))
		result = mailbox_write_blocked();

	if (unlock_fd(mailbox_fd)) return 1;

	return result;
}

int mailbox_close(void)
{
	if (mailbox_fd < 0) return 0;

	return close(mailbox_fd);
}
