/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Low level character input from the input file.
 * We use these special purpose routines which optimize moving
 * both forward and backward from the current read pointer.
 */

#include <sys/stat.h>

#include "less.h"

extern dev_t curr_dev;
extern ino_t curr_ino;
extern int less_is_more;

typedef off_t BLOCKNUM;

int ignore_eoi;

/*
 * Pool of buffers holding the most recently used blocks of the input file.
 * The buffer pool is kept as a doubly-linked circular list,
 * in order from most- to least-recently used.
 * The circular list is anchored by the file state "thisfile".
 */
struct bufnode {
	struct bufnode *next, *prev;
	struct bufnode *hnext, *hprev;
};

#define	LBUFSIZE	8192
struct buf {
	struct bufnode node;
	BLOCKNUM block;
	unsigned int datasize;
	unsigned char data[LBUFSIZE];
};
#define	bufnode_buf(bn)  ((struct buf *)bn)

/*
 * The file state is maintained in a filestate structure.
 * A pointer to the filestate is kept in the ifile structure.
 */
#define	BUFHASH_SIZE	64
struct filestate {
	struct bufnode buflist;
	struct bufnode hashtbl[BUFHASH_SIZE];
	int file;
	int flags;
	off_t fpos;
	int nbufs;
	BLOCKNUM block;
	unsigned int offset;
	off_t fsize;
};

#define	ch_bufhead	thisfile->buflist.next
#define	ch_buftail	thisfile->buflist.prev
#define	ch_nbufs	thisfile->nbufs
#define	ch_block	thisfile->block
#define	ch_offset	thisfile->offset
#define	ch_fpos		thisfile->fpos
#define	ch_fsize	thisfile->fsize
#define	ch_flags	thisfile->flags
#define	ch_file		thisfile->file

#define	END_OF_CHAIN	(&thisfile->buflist)
#define	END_OF_HCHAIN(h) (&thisfile->hashtbl[h])
#define	BUFHASH(blk)	((blk) & (BUFHASH_SIZE-1))

/*
 * Macros to manipulate the list of buffers in thisfile->buflist.
 */
#define	FOR_BUFS(bn) \
	for ((bn) = ch_bufhead;  (bn) != END_OF_CHAIN;  (bn) = (bn)->next)

#define	BUF_RM(bn) \
	(bn)->next->prev = (bn)->prev; \
	(bn)->prev->next = (bn)->next;

#define	BUF_INS_HEAD(bn) \
	(bn)->next = ch_bufhead; \
	(bn)->prev = END_OF_CHAIN; \
	ch_bufhead->prev = (bn); \
	ch_bufhead = (bn);

#define	BUF_INS_TAIL(bn) \
	(bn)->next = END_OF_CHAIN; \
	(bn)->prev = ch_buftail; \
	ch_buftail->next = (bn); \
	ch_buftail = (bn);

/*
 * Macros to manipulate the list of buffers in thisfile->hashtbl[n].
 */
#define	FOR_BUFS_IN_CHAIN(h, bn) \
	for ((bn) = thisfile->hashtbl[h].hnext;  \
	    (bn) != END_OF_HCHAIN(h); (bn) = (bn)->hnext)

#define	BUF_HASH_RM(bn) \
	(bn)->hnext->hprev = (bn)->hprev; \
	(bn)->hprev->hnext = (bn)->hnext;

#define	BUF_HASH_INS(bn, h) \
	(bn)->hnext = thisfile->hashtbl[h].hnext; \
	(bn)->hprev = END_OF_HCHAIN(h); \
	thisfile->hashtbl[h].hnext->hprev = (bn); \
	thisfile->hashtbl[h].hnext = (bn);

static struct filestate *thisfile;
static int ch_ungotchar = -1;
static int maxbufs = -1;

extern int autobuf;
extern volatile sig_atomic_t sigs;
extern int secure;
extern int screen_trashed;
extern int follow_mode;
extern IFILE curr_ifile;
extern int logfile;
extern char *namelogfile;

static int ch_addbuf(void);


/*
 * Get the character pointed to by the read pointer.
 */
int
ch_get(void)
{
	struct buf *bp;
	struct bufnode *bn;
	int n;
	int slept;
	int h;
	off_t pos;
	off_t len;

	if (thisfile == NULL)
		return (EOI);

	/*
	 * Quick check for the common case where
	 * the desired char is in the head buffer.
	 */
	if (ch_bufhead != END_OF_CHAIN) {
		bp = bufnode_buf(ch_bufhead);
		if (ch_block == bp->block && ch_offset < bp->datasize)
			return (bp->data[ch_offset]);
	}

	slept = FALSE;

	/*
	 * Look for a buffer holding the desired block.
	 */
	h = BUFHASH(ch_block);
	FOR_BUFS_IN_CHAIN(h, bn) {
		bp = bufnode_buf(bn);
		if (bp->block == ch_block) {
			if (ch_offset >= bp->datasize)
				/*
				 * Need more data in this buffer.
				 */
				break;
			goto found;
		}
	}
	if (bn == END_OF_HCHAIN(h)) {
		/*
		 * Block is not in a buffer.
		 * Take the least recently used buffer
		 * and read the desired block into it.
		 * If the LRU buffer has data in it,
		 * then maybe allocate a new buffer.
		 */
		if (ch_buftail == END_OF_CHAIN ||
		    bufnode_buf(ch_buftail)->block != -1) {
			/*
			 * There is no empty buffer to use.
			 * Allocate a new buffer if:
			 * 1. We can't seek on this file and -b is not in
			 *    effect; or
			 * 2. We haven't allocated the max buffers for this
			 *    file yet.
			 */
			if ((autobuf && !(ch_flags & CH_CANSEEK)) ||
			    (maxbufs < 0 || ch_nbufs < maxbufs))
				if (ch_addbuf())
					/*
					 * Allocation failed: turn off autobuf.
					 */
					autobuf = OPT_OFF;
		}
		bn = ch_buftail;
		bp = bufnode_buf(bn);
		BUF_HASH_RM(bn); /* Remove from old hash chain. */
		bp->block = ch_block;
		bp->datasize = 0;
		BUF_HASH_INS(bn, h); /* Insert into new hash chain. */
	}

read_more:
	pos = (ch_block * LBUFSIZE) + bp->datasize;
	if ((len = ch_length()) != -1 && pos >= len)
		/*
		 * At end of file.
		 */
		return (EOI);

	if (pos != ch_fpos) {
		/*
		 * Not at the correct position: must seek.
		 * If input is a pipe, we're in trouble (can't seek on a pipe).
		 * Some data has been lost: just return "?".
		 */
		if (!(ch_flags & CH_CANSEEK))
			return ('?');
		if (lseek(ch_file, (off_t)pos, SEEK_SET) == (off_t)-1) {
			error("seek error", NULL);
			clear_eol();
			return (EOI);
		}
		ch_fpos = pos;
	}

	/*
	 * Read the block.
	 * If we read less than a full block, that's ok.
	 * We use partial block and pick up the rest next time.
	 */
	if (ch_ungotchar != -1) {
		bp->data[bp->datasize] = (unsigned char)ch_ungotchar;
		n = 1;
		ch_ungotchar = -1;
	} else {
		n = iread(ch_file, &bp->data[bp->datasize],
		    (unsigned int)(LBUFSIZE - bp->datasize));
	}

	if (n == READ_INTR)
		return (EOI);
	if (n < 0) {
		error("read error", NULL);
		clear_eol();
		n = 0;
	}

	/*
	 * If we have a log file, write the new data to it.
	 */
	if (!secure && logfile >= 0 && n > 0)
		(void) write(logfile, (char *)&bp->data[bp->datasize], n);

	ch_fpos += n;
	bp->datasize += n;

	/*
	 * If we have read to end of file, set ch_fsize to indicate
	 * the position of the end of file.
	 */
	if (n == 0) {
		ch_fsize = pos;
		if (ignore_eoi) {
			/*
			 * We are ignoring EOF.
			 * Wait a while, then try again.
			 */
			if (!slept) {
				PARG parg;
				parg.p_string = wait_message();
				ierror("%s", &parg);
			}
			sleep(1);
			slept = TRUE;

			if (follow_mode == FOLLOW_NAME) {
				/*
				 * See whether the file's i-number has changed.
				 * If so, force the file to be closed and
				 * reopened.
				 */
				struct stat st;
				int r = stat(get_filename(curr_ifile), &st);
				if (r == 0 && (st.st_ino != curr_ino ||
				    st.st_dev != curr_dev)) {
					/*
					 * screen_trashed=2 causes
					 * make_display to reopen the file.
					 */
					screen_trashed = 2;
					return (EOI);
				}
			}
		}
		if (sigs)
			return (EOI);
	}

found:
	if (ch_bufhead != bn) {
		/*
		 * Move the buffer to the head of the buffer chain.
		 * This orders the buffer chain, most- to least-recently used.
		 */
		BUF_RM(bn);
		BUF_INS_HEAD(bn);

		/*
		 * Move to head of hash chain too.
		 */
		BUF_HASH_RM(bn);
		BUF_HASH_INS(bn, h);
	}

	if (ch_offset >= bp->datasize)
		/*
		 * After all that, we still don't have enough data.
		 * Go back and try again.
		 */
		goto read_more;

	return (bp->data[ch_offset]);
}

/*
 * ch_ungetchar is a rather kludgy and limited way to push
 * a single char onto an input file descriptor.
 */
void
ch_ungetchar(int c)
{
	if (c != -1 && ch_ungotchar != -1)
		error("ch_ungetchar overrun", NULL);
	ch_ungotchar = c;
}

/*
 * Close the logfile.
 * If we haven't read all of standard input into it, do that now.
 */
void
end_logfile(void)
{
	static int tried = FALSE;

	if (logfile < 0)
		return;
	if (!tried && ch_fsize == -1) {
		tried = TRUE;
		ierror("Finishing logfile", NULL);
		while (ch_forw_get() != EOI)
			if (ABORT_SIGS())
				break;
	}
	close(logfile);
	logfile = -1;
	namelogfile = NULL;
}

/*
 * Start a log file AFTER less has already been running.
 * Invoked from the - command; see toggle_option().
 * Write all the existing buffered data to the log file.
 */
void
sync_logfile(void)
{
	struct buf *bp;
	struct bufnode *bn;
	int warned = FALSE;
	BLOCKNUM block;
	BLOCKNUM nblocks;

	nblocks = (ch_fpos + LBUFSIZE - 1) / LBUFSIZE;
	for (block = 0;  block < nblocks;  block++) {
		int wrote = FALSE;
		FOR_BUFS(bn) {
			bp = bufnode_buf(bn);
			if (bp->block == block) {
				(void) write(logfile, (char *)bp->data,
				    bp->datasize);
				wrote = TRUE;
				break;
			}
		}
		if (!wrote && !warned) {
			error("Warning: log file is incomplete", NULL);
			warned = TRUE;
		}
	}
}

/*
 * Determine if a specific block is currently in one of the buffers.
 */
static int
buffered(BLOCKNUM block)
{
	struct buf *bp;
	struct bufnode *bn;
	int h;

	h = BUFHASH(block);
	FOR_BUFS_IN_CHAIN(h, bn) {
		bp = bufnode_buf(bn);
		if (bp->block == block)
			return (TRUE);
	}
	return (FALSE);
}

/*
 * Seek to a specified position in the file.
 * Return 0 if successful, non-zero if can't seek there.
 */
int
ch_seek(off_t pos)
{
	BLOCKNUM new_block;
	off_t len;

	if (thisfile == NULL)
		return (0);

	len = ch_length();
	if (pos < ch_zero() || (len != -1 && pos > len))
		return (1);

	new_block = pos / LBUFSIZE;
	if (!(ch_flags & CH_CANSEEK) && pos != ch_fpos &&
	    !buffered(new_block)) {
		if (ch_fpos > pos)
			return (1);
		while (ch_fpos < pos) {
			if (ch_forw_get() == EOI)
				return (1);
			if (ABORT_SIGS())
				return (1);
		}
		return (0);
	}
	/*
	 * Set read pointer.
	 */
	ch_block = new_block;
	ch_offset = pos % LBUFSIZE;
	return (0);
}

/*
 * Seek to the end of the file.
 */
int
ch_end_seek(void)
{
	off_t len;

	if (thisfile == NULL)
		return (0);

	if (ch_flags & CH_CANSEEK)
		ch_fsize = filesize(ch_file);

	len = ch_length();
	if (len != -1)
		return (ch_seek(len));

	/*
	 * Do it the slow way: read till end of data.
	 */
	while (ch_forw_get() != EOI)
		if (ABORT_SIGS())
			return (1);
	return (0);
}

/*
 * Seek to the beginning of the file, or as close to it as we can get.
 * We may not be able to seek there if input is a pipe and the
 * beginning of the pipe is no longer buffered.
 */
int
ch_beg_seek(void)
{
	struct bufnode *bn;
	struct bufnode *firstbn;

	/*
	 * Try a plain ch_seek first.
	 */
	if (ch_seek(ch_zero()) == 0)
		return (0);

	/*
	 * Can't get to position 0.
	 * Look thru the buffers for the one closest to position 0.
	 */
	firstbn = ch_bufhead;
	if (firstbn == END_OF_CHAIN)
		return (1);
	FOR_BUFS(bn) {
		if (bufnode_buf(bn)->block < bufnode_buf(firstbn)->block)
			firstbn = bn;
	}
	ch_block = bufnode_buf(firstbn)->block;
	ch_offset = 0;
	return (0);
}

/*
 * Return the length of the file, if known.
 */
off_t
ch_length(void)
{
	if (thisfile == NULL)
		return (-1);
	if (ignore_eoi)
		return (-1);
	if (ch_flags & CH_NODATA)
		return (0);
	return (ch_fsize);
}

/*
 * Return the current position in the file.
 */
off_t
ch_tell(void)
{
	if (thisfile == NULL)
		return (-1);
	return ((ch_block * LBUFSIZE) + ch_offset);
}

/*
 * Get the current char and post-increment the read pointer.
 */
int
ch_forw_get(void)
{
	int c;

	if (thisfile == NULL)
		return (EOI);
	c = ch_get();
	if (c == EOI)
		return (EOI);
	if (ch_offset < LBUFSIZE-1) {
		ch_offset++;
	} else {
		ch_block ++;
		ch_offset = 0;
	}
	return (c);
}

/*
 * Pre-decrement the read pointer and get the new current char.
 */
int
ch_back_get(void)
{
	if (thisfile == NULL)
		return (EOI);
	if (ch_offset > 0) {
		ch_offset --;
	} else {
		if (ch_block <= 0)
			return (EOI);
		if (!(ch_flags & CH_CANSEEK) && !buffered(ch_block-1))
			return (EOI);
		ch_block--;
		ch_offset = LBUFSIZE-1;
	}
	return (ch_get());
}

/*
 * Set max amount of buffer space.
 * bufspace is in units of 1024 bytes.  -1 mean no limit.
 */
void
ch_setbufspace(int bufspace)
{
	if (bufspace < 0) {
		maxbufs = -1;
	} else {
		maxbufs = ((bufspace * 1024) + LBUFSIZE-1) / LBUFSIZE;
		if (maxbufs < 1)
			maxbufs = 1;
	}
}

/*
 * Flush (discard) any saved file state, including buffer contents.
 */
void
ch_flush(void)
{
	struct bufnode *bn;

	if (thisfile == NULL)
		return;

	if (!(ch_flags & CH_CANSEEK)) {
		/*
		 * If input is a pipe, we don't flush buffer contents,
		 * since the contents can't be recovered.
		 */
		ch_fsize = -1;
		return;
	}

	/*
	 * Initialize all the buffers.
	 */
	FOR_BUFS(bn) {
		bufnode_buf(bn)->block = -1;
	}

	/*
	 * Figure out the size of the file, if we can.
	 */
	ch_fsize = filesize(ch_file);

	/*
	 * Seek to a known position: the beginning of the file.
	 */
	ch_fpos = 0;
	ch_block = 0; /* ch_fpos / LBUFSIZE; */
	ch_offset = 0; /* ch_fpos % LBUFSIZE; */

#if 1
	/*
	 * This is a kludge to workaround a Linux kernel bug: files in
	 * /proc have a size of 0 according to fstat() but have readable
	 * data.  They are sometimes, but not always, seekable.
	 * Force them to be non-seekable here.
	 */
	if (ch_fsize == 0) {
		ch_fsize = -1;
		ch_flags &= ~CH_CANSEEK;
	}
#endif

	if (lseek(ch_file, (off_t)0, SEEK_SET) == (off_t)-1) {
		/*
		 * Warning only; even if the seek fails for some reason,
		 * there's a good chance we're at the beginning anyway.
		 * {{ I think this is bogus reasoning. }}
		 */
		error("seek error to 0", NULL);
	}
}

/*
 * Allocate a new buffer.
 * The buffer is added to the tail of the buffer chain.
 */
static int
ch_addbuf(void)
{
	struct buf *bp;
	struct bufnode *bn;

	/*
	 * Allocate and initialize a new buffer and link it
	 * onto the tail of the buffer list.
	 */
	bp = calloc(1, sizeof (struct buf));
	if (bp == NULL)
		return (1);
	ch_nbufs++;
	bp->block = -1;
	bn = &bp->node;

	BUF_INS_TAIL(bn);
	BUF_HASH_INS(bn, 0);
	return (0);
}

/*
 *
 */
static void
init_hashtbl(void)
{
	int h;

	for (h = 0; h < BUFHASH_SIZE; h++) {
		thisfile->hashtbl[h].hnext = END_OF_HCHAIN(h);
		thisfile->hashtbl[h].hprev = END_OF_HCHAIN(h);
	}
}

/*
 * Delete all buffers for this file.
 */
static void
ch_delbufs(void)
{
	struct bufnode *bn;

	while (ch_bufhead != END_OF_CHAIN) {
		bn = ch_bufhead;
		BUF_RM(bn);
		free(bufnode_buf(bn));
	}
	ch_nbufs = 0;
	init_hashtbl();
}

/*
 * Is it possible to seek on a file descriptor?
 */
int
seekable(int f)
{
	return (lseek(f, (off_t)1, SEEK_SET) != (off_t)-1);
}

/*
 * Force EOF to be at the current read position.
 * This is used after an ignore_eof read, during which the EOF may change.
 */
void
ch_set_eof(void)
{
	ch_fsize = ch_fpos;
}


/*
 * Initialize file state for a new file.
 */
void
ch_init(int f, int flags)
{
	/*
	 * See if we already have a filestate for this file.
	 */
	thisfile = get_filestate(curr_ifile);
	if (thisfile == NULL) {
		/*
		 * Allocate and initialize a new filestate.
		 */
		thisfile = calloc(1, sizeof (struct filestate));
		thisfile->buflist.next = thisfile->buflist.prev = END_OF_CHAIN;
		thisfile->nbufs = 0;
		thisfile->flags = 0;
		thisfile->fpos = 0;
		thisfile->block = 0;
		thisfile->offset = 0;
		thisfile->file = -1;
		thisfile->fsize = -1;
		ch_flags = flags;
		init_hashtbl();
		/*
		 * Try to seek; set CH_CANSEEK if it works.
		 */
		if ((flags & CH_CANSEEK) && !seekable(f))
			ch_flags &= ~CH_CANSEEK;
		set_filestate(curr_ifile, (void *) thisfile);
	}
	if (thisfile->file == -1)
		thisfile->file = f;
	ch_flush();
}

/*
 * Close a filestate.
 */
void
ch_close(void)
{
	int keepstate = FALSE;

	if (thisfile == NULL)
		return;

	if (ch_flags & (CH_CANSEEK|CH_POPENED|CH_HELPFILE)) {
		/*
		 * We can seek or re-open, so we don't need to keep buffers.
		 */
		ch_delbufs();
	} else {
		keepstate = TRUE;
	}
	if (!(ch_flags & CH_KEEPOPEN)) {
		/*
		 * We don't need to keep the file descriptor open
		 * (because we can re-open it.)
		 * But don't really close it if it was opened via popen(),
		 * because pclose() wants to close it.
		 */
		if (!(ch_flags & CH_POPENED))
			close(ch_file);
		ch_file = -1;
	} else {
		keepstate = TRUE;
	}
	if (!keepstate) {
		/*
		 * We don't even need to keep the filestate structure.
		 */
		free(thisfile);
		thisfile = NULL;
		set_filestate(curr_ifile, NULL);
	}
}

/*
 * Return ch_flags for the current file.
 */
int
ch_getflags(void)
{
	if (thisfile == NULL)
		return (0);
	return (ch_flags);
}
