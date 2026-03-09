/* Public domain. */

#ifndef _LINUX_SEQ_BUF_H
#define _LINUX_SEQ_BUF_H

#include <linux/types.h>

struct seq_buf {
	char	*buf;
	size_t	size;
	size_t 	pos;
	bool	overflowed;
};

#define DECLARE_SEQ_BUF(name, bsize)	\
	struct seq_buf name = { (char[bsize]) {0}, bsize, 0, false }

static inline char *
seq_buf_str(struct seq_buf *s)
{
	s->buf[s->pos] = '\0';
	return s->buf;
}

static inline void
seq_buf_clear(struct seq_buf *s)
{
	s->pos = 0;
	s->buf[0] = '\0';
}

static inline bool
seq_buf_has_overflowed(struct seq_buf *s)
{
	return s->overflowed;
}

void seq_buf_printf(struct seq_buf *, const char *, ...);

#endif
