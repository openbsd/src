/*	$OpenBSD: getchar.c,v 1.1.1.1 1996/09/07 21:40:26 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * getchar.c
 *
 * functions related with getting a character from the user/mapping/redo/...
 *
 * manipulations with redo buffer and stuff buffer
 * mappings and abbreviations
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

/*
 * structure used to store one block of the stuff/redo/macro buffers
 */
struct bufblock
{
		struct bufblock *b_next;		/* pointer to next bufblock */
		char_u			b_str[1];		/* contents (actually longer) */
};

#define MINIMAL_SIZE 20 				/* minimal size for b_str */

/*
 * header used for the stuff buffer and the redo buffer
 */
struct buffheader
{
		struct bufblock bh_first;		/* first (dummy) block of list */
		struct bufblock *bh_curr;		/* bufblock for appending */
		int 			bh_index;		/* index for reading */
		int 			bh_space;		/* space in bh_curr for appending */
};

static struct buffheader stuffbuff = {{NULL, {NUL}}, NULL, 0, 0};
static struct buffheader redobuff = {{NULL, {NUL}}, NULL, 0, 0};
static struct buffheader old_redobuff = {{NULL, {NUL}}, NULL, 0, 0};
static struct buffheader recordbuff = {{NULL, {NUL}}, NULL, 0, 0};

/*
 * when block_redo is TRUE redo buffer will not be changed
 * used by edit() to repeat insertions and 'V' command for redoing
 */
static int		block_redo = FALSE;

/*
 * structure used for mapping
 */
struct mapblock
{
	struct mapblock *m_next;		/* next mapblock */
	char_u			*m_keys;		/* mapped from */
	int				 m_keylen;		/* strlen(m_keys) */
	char_u			*m_str; 		/* mapped to */
	int 			 m_mode;		/* valid mode */
	int				 m_noremap;		/* if non-zero no re-mapping for m_str */
};

static struct mapblock maplist = {NULL, NULL, 0, NULL, 0, 0};
									/* first dummy entry in maplist */

/*
 * variables used by vgetorpeek() and flush_buffers()
 *
 * typebuf[] contains all characters that are not consumed yet.
 * typebuf[typeoff] is the first valid character in typebuf[].
 * typebuf[typeoff + typelen - 1] is the last valid char.
 * typebuf[typeoff + typelen] must be NUL.
 * The part in front may contain the result of mappings, abbreviations and
 * @a commands. The length of this part is typemaplen.
 * After it are characters that come from the terminal.
 * no_abbr_cnt is the number of characters in typebuf that should not be
 * considered for abbreviations.
 * Some parts of typebuf may not be mapped. These parts are remembered in
 * noremapbuf, which is the same length as typebuf and contains TRUE for the
 * characters that are not to be remapped. noremapbuf[typeoff] is the first
 * valid flag.
 * (typebuf has been put in globals.h, because check_termcode() needs it).
 */
static char_u	*noremapbuf = NULL;       /* flags for typeahead characters */
#define TYPELEN_INIT	(3 * (MAXMAPLEN + 3))
static char_u	typebuf_init[TYPELEN_INIT];			/* initial typebuf */
static char_u	noremapbuf_init[TYPELEN_INIT];		/* initial noremapbuf */

static int		typemaplen = 0;		/* nr of mapped characters in typebuf */
static int		no_abbr_cnt = 0;	/* nr of chars without abbrev. in typebuf */
static int		last_recorded_len = 0;	/* number of last recorded chars */

static void		free_buff __ARGS((struct buffheader *));
static char_u	*get_bufcont __ARGS((struct buffheader *, int));
static void		add_buff __ARGS((struct buffheader *, char_u *));
static void		add_num_buff __ARGS((struct buffheader *, long));
static void		add_char_buff __ARGS((struct buffheader *, int));
static int		read_stuff __ARGS((int));
static void		start_stuff __ARGS((void));
static int		read_redo __ARGS((int, int));
static void		copy_redo __ARGS((int));
static void		init_typebuf __ARGS((void));
static void		gotchars __ARGS((char_u *, int));
static int		vgetorpeek __ARGS((int));
static int		inchar __ARGS((char_u *, int, long));
static void		map_free __ARGS((struct mapblock *));
static void		showmap __ARGS((struct mapblock *));

/*
 * free and clear a buffer
 */
	static void
free_buff(buf)
	struct buffheader *buf;
{
		register struct bufblock *p, *np;

		for (p = buf->bh_first.b_next; p != NULL; p = np)
		{
				np = p->b_next;
				vim_free(p);
		}
		buf->bh_first.b_next = NULL;
}

/*
 * return the contents of a buffer as a single string
 */
	static char_u *
get_bufcont(buffer, dozero)
	struct buffheader	*buffer;
	int					dozero;		/* count == zero is not an error */
{
	long_u			count = 0;
	char_u			*p = NULL;
	char_u			*p2;
	char_u			*str;
	struct bufblock	*bp;

/* compute the total length of the string */
	for (bp = buffer->bh_first.b_next; bp != NULL; bp = bp->b_next)
		count += STRLEN(bp->b_str);

	if ((count || dozero) && (p = lalloc(count + 1, TRUE)) != NULL)
	{
		p2 = p;
		for (bp = buffer->bh_first.b_next; bp != NULL; bp = bp->b_next)
			for (str = bp->b_str; *str; )
				*p2++ = *str++;
		*p2 = NUL;
	}
	return (p);
}

/*
 * return the contents of the record buffer as a single string
 *	and clear the record buffer
 */
	char_u *
get_recorded()
{
	char_u *p;
	size_t	len;

	p = get_bufcont(&recordbuff, TRUE);
	free_buff(&recordbuff);
	/*
	 * Remove the characters that were added the last time, these must be the
	 * (possibly mapped) characters that stopped recording.
	 */
	len = STRLEN(p);
	if ((int)len >= last_recorded_len)
		p[len - last_recorded_len] = NUL;
	return (p);
}

/*
 * return the contents of the redo buffer as a single string
 */
	char_u *
get_inserted()
{
		return(get_bufcont(&redobuff, FALSE));
}

/*
 * add string "s" after the current block of buffer "buf"
 */
	static void
add_buff(buf, s)
	register struct buffheader	*buf;
	char_u						*s;
{
	struct bufblock *p;
	long_u 			n;
	long_u 			len;

	if ((n = STRLEN(s)) == 0)				/* don't add empty strings */
		return;

	if (buf->bh_first.b_next == NULL)		/* first add to list */
	{
		buf->bh_space = 0;
		buf->bh_curr = &(buf->bh_first);
	}
	else if (buf->bh_curr == NULL)			/* buffer has already been read */
	{
		EMSG("Add to read buffer");
		return;
	}
	else if (buf->bh_index != 0)
		STRCPY(buf->bh_first.b_next->b_str,
								 buf->bh_first.b_next->b_str + buf->bh_index);
	buf->bh_index = 0;

	if (buf->bh_space >= (int)n)
	{
		strcat((char *)buf->bh_curr->b_str, (char *)s);
		buf->bh_space -= n;
	}
	else
	{
		if (n < MINIMAL_SIZE)
			len = MINIMAL_SIZE;
		else
			len = n;
		p = (struct bufblock *)lalloc((long_u)(sizeof(struct bufblock) + len), TRUE);
		if (p == NULL)
			return; /* no space, just forget it */
		buf->bh_space = len - n;
		STRCPY(p->b_str, s);

		p->b_next = buf->bh_curr->b_next;
		buf->bh_curr->b_next = p;
		buf->bh_curr = p;
	}
	return;
}

	static void
add_num_buff(buf, n)
	struct buffheader *buf;
	long 			  n;
{
		char_u	number[32];

		sprintf((char *)number, "%ld", n);
		add_buff(buf, number);
}

	static void
add_char_buff(buf, c)
	struct buffheader *buf;
	int 			  c;
{
	char_u	temp[4];

	/*
	 * translate special key code into three byte sequence
	 */
	if (IS_SPECIAL(c) || c == K_SPECIAL || c == NUL)
	{
		temp[0] = K_SPECIAL;
		temp[1] = K_SECOND(c);
		temp[2] = K_THIRD(c);
		temp[3] = NUL;
	}
	else
	{
		temp[0] = c;
		temp[1] = NUL;
	}
	add_buff(buf, temp);
}

/*
 * get one character from the stuff buffer
 * If advance == TRUE go to the next char.
 */
	static int
read_stuff(advance)
	int			advance;
{
	register char_u c;
	register struct bufblock *curr;


	if (stuffbuff.bh_first.b_next == NULL)	/* buffer is empty */
		return NUL;

	curr = stuffbuff.bh_first.b_next;
	c = curr->b_str[stuffbuff.bh_index];

	if (advance)
	{
		if (curr->b_str[++stuffbuff.bh_index] == NUL)
		{
			stuffbuff.bh_first.b_next = curr->b_next;
			vim_free(curr);
			stuffbuff.bh_index = 0;
		}
	}
	return c;
}

/*
 * prepare stuff buffer for reading (if it contains something)
 */
	static void
start_stuff()
{
	if (stuffbuff.bh_first.b_next != NULL)
	{
		stuffbuff.bh_curr = &(stuffbuff.bh_first);
		stuffbuff.bh_space = 0;
	}
}

/*
 * check if the stuff buffer is empty
 */
	int
stuff_empty()
{
	return (stuffbuff.bh_first.b_next == NULL);
}

/*
 * Remove the contents of the stuff buffer and the mapped characters in the
 * typeahead buffer (used in case of an error). If 'typeahead' is true,
 * flush all typeahead characters (used when interrupted by a CTRL-C).
 */
	void
flush_buffers(typeahead)
	int typeahead;
{
	init_typebuf();

	start_stuff();
	while (read_stuff(TRUE) != NUL)
		;

	if (typeahead)			/* remove all typeahead */
	{
			/*
			 * We have to get all characters, because we may delete the first
			 * part of an escape sequence.
			 * In an xterm we get one char at a time and we have to get them
			 * all.
			 */
		while (inchar(typebuf, MAXMAPLEN, 10L))	
			;
		typeoff = MAXMAPLEN;
		typelen = 0;
	}
	else					/* remove mapped characters only */
	{
		typeoff += typemaplen;
		typelen -= typemaplen;
	}
	typemaplen = 0;
	no_abbr_cnt = 0;
}

/*
 * The previous contents of the redo buffer is kept in old_redobuffer.
 * This is used for the CTRL-O <.> command in insert mode.
 */
	void
ResetRedobuff()
{
	if (!block_redo)
	{
		free_buff(&old_redobuff);
		old_redobuff = redobuff;
		redobuff.bh_first.b_next = NULL;
	}
}

	void
AppendToRedobuff(s)
	char_u		   *s;
{
	if (!block_redo)
		add_buff(&redobuff, s);
}

	void
AppendCharToRedobuff(c)
	int			   c;
{
	if (!block_redo)
		add_char_buff(&redobuff, c);
}

	void
AppendNumberToRedobuff(n)
	long 			n;
{
	if (!block_redo)
		add_num_buff(&redobuff, n);
}

	void
stuffReadbuff(s)
	char_u		   *s;
{
	add_buff(&stuffbuff, s);
}

	void
stuffcharReadbuff(c)
	int			   c;
{
	add_char_buff(&stuffbuff, c);
}

	void
stuffnumReadbuff(n)
	long	n;
{
	add_num_buff(&stuffbuff, n);
}

/*
 * Read a character from the redo buffer.
 * The redo buffer is left as it is.
 * if init is TRUE, prepare for redo, return FAIL if nothing to redo, OK
 * otherwise
 * if old is TRUE, use old_redobuff instead of redobuff
 */
	static int
read_redo(init, old_redo)
	int			init;
	int			old_redo;
{
	static struct bufblock	*bp;
	static char_u			*p;
	int						c;

	if (init)
	{
		if (old_redo)
			bp = old_redobuff.bh_first.b_next;
		else
			bp = redobuff.bh_first.b_next;
		if (bp == NULL)
			return FAIL;
		p = bp->b_str;
		return OK;
	}
	if ((c = *p) != NUL)
	{
		if (c == K_SPECIAL)
		{
			c = TO_SPECIAL(p[1], p[2]);
			p += 2;
		}
		if (*++p == NUL && bp->b_next != NULL)
		{
			bp = bp->b_next;
			p = bp->b_str;
		}
	}
	return c;
}

/*
 * copy the rest of the redo buffer into the stuff buffer (could be done faster)
 * if old_redo is TRUE, use old_redobuff instead of redobuff
 */
	static void
copy_redo(old_redo)
	int		old_redo;
{
	register int c;

	while ((c = read_redo(FALSE, old_redo)) != NUL)
		stuffcharReadbuff(c);
}

/*
 * Stuff the redo buffer into the stuffbuff.
 * Insert the redo count into the command.
 * If 'old_redo' is TRUE, the last but one command is repeated
 * instead of the last command (inserting text). This is used for
 * CTRL-O <.> in insert mode
 *
 * return FAIL for failure, OK otherwise
 */
	int
start_redo(count, old_redo)
	long	count;
	int		old_redo;
{
	register int c;

	if (read_redo(TRUE, old_redo) == FAIL)	/* init the pointers; return if nothing to redo */
		return FAIL;

	c = read_redo(FALSE, old_redo);

/* copy the buffer name, if present */
	if (c == '"')
	{
		add_buff(&stuffbuff, (char_u *)"\"");
		c = read_redo(FALSE, old_redo);

/* if a numbered buffer is used, increment the number */
		if (c >= '1' && c < '9')
			++c;
		add_char_buff(&stuffbuff, c);
		c = read_redo(FALSE, old_redo);
	}

	if (c == 'v')	/* redo Visual */
	{
		VIsual = curwin->w_cursor;
		VIsual_active = TRUE;
		redo_VIsual_busy = TRUE;
		c = read_redo(FALSE, old_redo);
	}

/* try to enter the count (in place of a previous count) */
	if (count)
	{
		while (isdigit(c))		/* skip "old" count */
			c = read_redo(FALSE, old_redo);
		add_num_buff(&stuffbuff, count);
	}

/* copy from the redo buffer into the stuff buffer */
	add_char_buff(&stuffbuff, c);
	copy_redo(old_redo);
	return OK;
}

/*
 * Repeat the last insert (R, o, O, a, A, i or I command) by stuffing
 * the redo buffer into the stuffbuff.
 * return FAIL for failure, OK otherwise
 */
	int
start_redo_ins()
{
	register int c;

	if (read_redo(TRUE, FALSE) == FAIL)
		return FAIL;
	start_stuff();

/* skip the count and the command character */
	while ((c = read_redo(FALSE, FALSE)) != NUL)
	{
		c = TO_UPPER(c);
		if (vim_strchr((char_u *)"AIRO", c) != NULL)
		{
			if (c == 'O')
				stuffReadbuff(NL_STR);
			break;
		}
	}

/* copy the typed text from the redo buffer into the stuff buffer */
	copy_redo(FALSE);
	block_redo = TRUE;
	return OK;
}

	void
set_redo_ins()
{
	block_redo = TRUE;
}

	void
stop_redo_ins()
{
	block_redo = FALSE;
}

/*
 * Initialize typebuf to point to typebuf_init.
 * Alloc() cannot be used here: In out-of-memory situations it would
 * be impossible to type anything.
 */
	static void
init_typebuf()
{
	if (typebuf == NULL)
	{
		typebuf = typebuf_init;
		noremapbuf = noremapbuf_init;
		typebuflen = TYPELEN_INIT;
		typelen = 0;
		typeoff = 0;
	}
}

/*
 * insert a string in position 'offset' in the typeahead buffer (for "@r"
 * and ":normal" command, vgetorpeek() and check_termcode())
 *
 * If noremap is 0, new string can be mapped again.
 * If noremap is -1, new string cannot be mapped again.
 * If noremap is >0, that many characters of the new string cannot be mapped.
 *
 * If nottyped is TRUE, the string does not return KeyTyped (don't use when
 * offset is non-zero!).
 *
 * return FAIL for failure, OK otherwise
 */
	int
ins_typebuf(str, noremap, offset, nottyped)
	char_u	*str;
	int		noremap;
	int		offset;
	int		nottyped;
{
	register char_u	*s1, *s2;
	register int	newlen;
	register int	addlen;
	register int	i;
	register int	newoff;

	init_typebuf();

	addlen = STRLEN(str);
	/*
	 * Easy case: there is room in front of typebuf[typeoff]
	 */
	if (offset == 0 && addlen <= typeoff)
	{
		typeoff -= addlen;
		vim_memmove(typebuf + typeoff, str, (size_t)addlen);
	}
	/*
	 * Need to allocate new buffer.
	 * In typebuf there must always be room for MAXMAPLEN + 4 characters.
	 * We add some extra room to avoid having to allocate too often.
	 */
	else
	{
		newoff = MAXMAPLEN + 4;
		newlen = typelen + addlen + newoff + 2 * (MAXMAPLEN + 4);
		if (newlen < 0)				/* string is getting too long */
		{
			emsg(e_toocompl);		/* also calls flush_buffers */
			setcursor();
			return FAIL;
		}
		s1 = alloc(newlen);
		if (s1 == NULL)				/* out of memory */
			return FAIL;
		s2 = alloc(newlen);
		if (s2 == NULL)				/* out of memory */
		{
			vim_free(s1);
			return FAIL;
		}
		typebuflen = newlen;

				/* copy the old chars, before the insertion point */
		vim_memmove(s1 + newoff, typebuf + typeoff, (size_t)offset);
				/* copy the new chars */
		vim_memmove(s1 + newoff + offset, str, (size_t)addlen);
				/* copy the old chars, after the insertion point, including
				 * the  NUL at the end */
		vim_memmove(s1 + newoff + offset + addlen, typebuf + typeoff + offset,
											  (size_t)(typelen - offset + 1));
		if (typebuf != typebuf_init)
			vim_free(typebuf);
		typebuf = s1;

		vim_memmove(s2 + newoff, noremapbuf + typeoff, (size_t)offset);
		vim_memmove(s2 + newoff + offset + addlen,
				   noremapbuf + typeoff + offset, (size_t)(typelen - offset));
		if (noremapbuf != noremapbuf_init)
			vim_free(noremapbuf);
		noremapbuf = s2;

		typeoff = newoff;
	}
	typelen += addlen;

	/*
	 * Adjust noremapbuf[] for the new characters:
	 * If noremap  < 0: all the new characters are flagged not remappable
	 * If noremap == 0: all the new characters are flagged mappable
	 * If noremap  > 0: 'noremap' characters are flagged not remappable, the
	 *					rest mappable
	 */
	if (noremap < 0)		/* length not specified */
		noremap = addlen;
	for (i = 0; i < addlen; ++i)
		noremapbuf[typeoff + i + offset] = (noremap-- > 0);

					/* this is only correct for offset == 0! */
	if (nottyped)						/* the inserted string is not typed */
		typemaplen += addlen;
	if (no_abbr_cnt && offset == 0)		/* and not used for abbreviations */
		no_abbr_cnt += addlen;

	return OK;
}

/*
 * Return TRUE if there are no characters in the typeahead buffer that have
 * not been typed (result from a mapping or come from ":normal").
 */
	int
typebuf_typed()
{
	return typemaplen == 0;
}

/*
 * remove "len" characters from typebuf[typeoff + offset]
 */
	void
del_typebuf(len, offset)
	int	len;
	int	offset;
{
	int		i;

	typelen -= len;
	/*
	 * Easy case: Just increase typeoff.
	 */
	if (offset == 0 && typebuflen - (typeoff + len) >= MAXMAPLEN + 3)
		typeoff += len;
	/*
	 * Have to move the characters in typebuf[] and noremapbuf[]
	 */
	else
	{
		i = typeoff + offset;
		/*
		 * Leave some extra room at the end to avoid reallocation.
		 */
		if (typeoff > MAXMAPLEN)
		{
			vim_memmove(typebuf + MAXMAPLEN, typebuf + typeoff, (size_t)offset);
			vim_memmove(noremapbuf + MAXMAPLEN, noremapbuf + typeoff,
															  (size_t)offset);
			typeoff = MAXMAPLEN;
		}
			/* adjust typebuf (include the NUL at the end) */
		vim_memmove(typebuf + typeoff + offset, typebuf + i + len,
											  (size_t)(typelen - offset + 1));
			/* adjust noremapbuf[] */
		vim_memmove(noremapbuf + typeoff + offset, noremapbuf + i + len,
												  (size_t)(typelen - offset));
	}

	if (typemaplen > offset)			/* adjust typemaplen */
	{
		if (typemaplen < offset + len)
			typemaplen = offset;
		else
			typemaplen -= len;
	}
	if (no_abbr_cnt > offset)			/* adjust no_abbr_cnt */
	{
		if (no_abbr_cnt < offset + len)
			no_abbr_cnt = offset;
		else
			no_abbr_cnt -= len;
	}
}

/*
 * Write typed characters to script file.
 * If recording is on put the character in the recordbuffer.
 */
	static void
gotchars(s, len)
	char_u	*s;
	int		len;
{
	int		c;
	char_u	buf[2];

	/* remember how many chars were last recorded */
	if (Recording)
		last_recorded_len += len;

	buf[1] = NUL;
	while (len--)
	{
		c = *s++;
		updatescript(c);

		if (Recording)
		{
			buf[0] = c;
			add_buff(&recordbuff, buf);
		}
	}

	/*
	 * Do not sync in insert mode, unless cursor key has been used.
	 * Also don't sync while reading a script file.
	 */
	if ((!(State & (INSERT + CMDLINE)) || arrow_used) &&
												  scriptin[curscript] == NULL)
		u_sync();
}

/*
 * open new script file for ":so!" command
 * return OK on success, FAIL on error
 */
	int
openscript(name)
	char_u *name;
{
	int oldcurscript;

	if (curscript + 1 == NSCRIPT)
	{
		emsg(e_nesting);
		return FAIL;
	}
	else
	{
		if (scriptin[curscript] != NULL)	/* already reading script */
			++curscript;
									/* use NameBuff for expanded name */
		expand_env(name, NameBuff, MAXPATHL);
		if ((scriptin[curscript] = fopen((char *)NameBuff, READBIN)) == NULL)
		{
			emsg2(e_notopen, name);
			if (curscript)
				--curscript;
			return FAIL;
		}
		/*
		 * With command ":g/pat/so! file" we have to execute the
		 * commands from the file now.
		 */
		if (global_busy)
		{
			State = NORMAL;
			oldcurscript = curscript;
			do
			{
				normal();
				vpeekc();			/* check for end of file */
			}
			while (scriptin[oldcurscript]);
			State = CMDLINE;
		}
	}
	return OK;
}

/*
 * updatescipt() is called when a character can be written into the script file
 * or when we have waited some time for a character (c == 0)
 *
 * All the changed memfiles are synced if c == 0 or when the number of typed
 * characters reaches 'updatecount' and 'updatecount' is non-zero.
 */
	void
updatescript(c)
	int c;
{
	static int		count = 0;

	if (c && scriptout)
		putc(c, scriptout);
	if (c == 0 || (p_uc > 0 && ++count >= p_uc))
	{
		ml_sync_all(c == 0, TRUE);
		count = 0;
	}
}

#define K_NEEDMORET -1			/* keylen value for incomplete key-code */
#define M_NEEDMORET -2			/* keylen value for incomplete mapping */

static int old_char = -1;		/* ungotten character */

	int
vgetc()
{
	int		c, c2;

	mod_mask = 0x0;
	last_recorded_len = 0;
	for (;;)					/* this is done twice if there are modifiers */
	{
		if (mod_mask)			/* no mapping after modifier has been read */
		{
			++no_mapping;
			++allow_keys;
		}
		c = vgetorpeek(TRUE);
		if (mod_mask)
		{
			--no_mapping;
			--allow_keys;
		}

		/* Get two extra bytes for special keys */
		if (c == K_SPECIAL)
		{
			++no_mapping;
			c2 = vgetorpeek(TRUE);		/* no mapping for these chars */
			c = vgetorpeek(TRUE);
			--no_mapping;
			if (c2 == KS_MODIFIER)
			{
				mod_mask = c;
				continue;
			}
			c = TO_SPECIAL(c2, c);
		}
#ifdef MSDOS
		/*
		 * If K_NUL was typed, it is replaced by K_NUL, 3 in mch_inchar().
		 * Delete the 3 here.
		 */
		else if (c == K_NUL && vpeekc() == 3)
			(void)vgetorpeek(TRUE);
#endif

		return check_shifted_spec_key(c);
	}
}

	int
vpeekc()
{
	return (vgetorpeek(FALSE));
}

/*
 * Call vpeekc() without causing anything to be mapped.
 * Return TRUE if a character is available, FALSE otherwise.
 */
	int
char_avail()
{
	int		retval;

	++no_mapping;
	retval = vgetorpeek(FALSE);
	--no_mapping;
	return (retval != NUL);
}

	void
vungetc(c)		/* unget one character (can only be done once!) */
	int		c;
{
	old_char = c;
}

/*
 * get a character: 1. from a previously ungotten character
 *					2. from the stuffbuffer
 *					3. from the typeahead buffer
 *					4. from the user
 *
 * if "advance" is TRUE (vgetc()):
 *		really get the character.
 *		KeyTyped is set to TRUE in the case the user typed the key.
 *		KeyStuffed is TRUE if the character comes from the stuff buffer.
 * if "advance" is FALSE (vpeekc()):
 *		just look whether there is a character available.
 */

	static int
vgetorpeek(advance)
	int		advance;
{
	register int	c, c1;
	int				keylen = 0;				/* init for gcc */
#ifdef AMIGA
	char_u			*s;
#endif
	register struct mapblock *mp;
	int				timedout = FALSE;		/* waited for more than 1 second
												for mapping to complete */
	int				mapdepth = 0;			/* check for recursive mapping */
	int				mode_deleted = FALSE;	/* set when mode has been deleted */
	int				local_State;
	register int	mlen;
	int				max_mlen;
	int				i;
#ifdef USE_GUI
	int				idx;
#endif

	/*
	 * VISUAL state is never set, it is used only here, therefore a check is
	 * made if NORMAL state is actually VISUAL state.
	 */
	local_State = State;
	if ((State & NORMAL) && VIsual_active)
		local_State = VISUAL;

/*
 * get a character: 1. from a previously ungotten character
 */
	if (old_char >= 0)
	{
		c = old_char;
		if (advance)
			old_char = -1;
		return c;
	}

	if (advance)
		KeyStuffed = FALSE;

	init_typebuf();
	start_stuff();
	if (advance && typemaplen == 0)
		Exec_reg = FALSE;
	do
	{
/*
 * get a character: 2. from the stuffbuffer
 */
		c = read_stuff(advance);
		if (c != NUL && !got_int)
		{
			if (advance)
			{
				KeyTyped = FALSE;
				KeyStuffed = TRUE;
			}
			if (no_abbr_cnt == 0)
				no_abbr_cnt = 1;		/* no abbreviations now */
		}
		else
		{
			/*
			 * Loop until we either find a matching mapped key, or we
			 * are sure that it is not a mapped key.
			 * If a mapped key sequence is found we go back to the start to
			 * try re-mapping.
			 */

			for (;;)
			{
				mch_breakcheck();			/* check for CTRL-C */
				if (got_int)
				{
					c = inchar(typebuf, MAXMAPLEN, 0L);	/* flush all input */
					/*
					 * If inchar returns TRUE (script file was active) or we are
					 * inside a mapping, get out of insert mode.
					 * Otherwise we behave like having gotten a CTRL-C.
					 * As a result typing CTRL-C in insert mode will
					 * really insert a CTRL-C.
					 */
					if ((c || typemaplen) && (State & (INSERT + CMDLINE)))
						c = ESC;
					else
						c = Ctrl('C');
					flush_buffers(TRUE);		/* flush all typeahead */
					break;
				}
				else if (typelen > 0)	/* check for a mappable key sequence */
				{
					/*
					 * walk through the maplist until we find an
					 * entry that matches.
					 *
					 * Don't look for mappings if:
					 * - timed out
					 * - no_mapping set: mapping disabled (e.g. for CTRL-V)
					 * - typebuf[typeoff] should not be remapped
					 * - in insert or cmdline mode and 'paste' option set
					 * - waiting for "hit return to continue" and CR or SPACE
					 *   typed
					 * - waiting for a char with --more--
					 * - in Ctrl-X mode, and we get a valid char for that mode
					 */
					mp = NULL;
					max_mlen = 0;
					if (!timedout && no_mapping == 0 && (typemaplen == 0 ||
								(p_remap && noremapbuf[typeoff] == FALSE))
							&& !(p_paste && (State & (INSERT + CMDLINE)))
							&& !(State == HITRETURN && (typebuf[typeoff] == CR
								|| typebuf[typeoff] == ' '))
							&& State != ASKMORE
#ifdef INSERT_EXPAND
							&& !(ctrl_x_mode && is_ctrl_x_key(typebuf[typeoff]))
#endif
							)
					{
						c1 = typebuf[typeoff];
#ifdef HAVE_LANGMAP
						LANGMAP_ADJUST(c1, TRUE);
#endif
						for (mp = maplist.m_next; mp; mp = mp->m_next)
						{
							/*
							 * Only consider an entry if
							 * - the first character matches and
							 * - it is not an abbreviation and
							 * - it is for the current state
							 */
							if (		mp->m_keys[0] == c1 &&
										!(mp->m_mode & ABBREV) &&
										(mp->m_mode & local_State))
							{
								int		n;
#ifdef HAVE_LANGMAP
								int		c2;
#endif

									/* find the match length of this mapping */
								for (mlen = 1; mlen < typelen; ++mlen)
								{
#ifdef HAVE_LANGMAP
									c2 = typebuf[typeoff + mlen];
									LANGMAP_ADJUST(c2, TRUE);
									if (mp->m_keys[mlen] != c2)
#else
									if (mp->m_keys[mlen] !=
													typebuf[typeoff + mlen])
#endif
										break;
								}

									/* if one of the typed keys cannot be
									 * remapped, skip the entry */
								for (n = 0; n < mlen; ++n)
									if (noremapbuf[typeoff + n] == TRUE)
										break;
								if (n != mlen)
									continue;

									/* (partly) match found */
								keylen = mp->m_keylen;
								if (mlen == (keylen > typelen ?
													typelen : keylen))
								{
										/* partial match, need more chars */
									if (keylen > typelen)
										keylen = M_NEEDMORET;
									break;	
								}
									/* no match, may have to check for
									 * termcode at next character */
								if (max_mlen < mlen)
									max_mlen = mlen;
							}
						}
					}
					if (mp == NULL)					/* no match found */
					{
							/*
							 * check if we have a terminal code, when
							 *	mapping is allowed,
							 *  keys have not been mapped,
							 *	and not an ESC sequence, not in insert mode or
							 *		p_ek is on,
							 *	and when not timed out,
							 */
						if ((no_mapping == 0 || allow_keys != 0) &&
														   (typemaplen == 0 ||
								(p_remap && noremapbuf[typeoff] == FALSE)) &&
											!timedout)
							keylen = check_termcode(max_mlen + 1);
						else
							keylen = 0;
						if (keylen == 0)		/* no matching terminal code */
						{
#ifdef AMIGA					/* check for window bounds report */
							if (typemaplen == 0 &&
											(typebuf[typeoff] & 0xff) == CSI)
							{
								for (s = typebuf + typeoff + 1;
										s < typebuf + typeoff + typelen &&
										(isdigit(*s) || *s == ';' || *s == ' ');
										++s)
									;
								if (*s == 'r' || *s == '|')	/* found one */
								{
									del_typebuf((int)(s + 1 -
													   (typebuf + typeoff)), 0);
										/* get size and redraw screen */
									set_winsize(0, 0, FALSE);
									continue;
								}
								if (*s == NUL)		/* need more characters */
									keylen = K_NEEDMORET;
							}
							if (keylen >= 0)
#endif
							{
/*
 * get a character: 3. from the typeahead buffer
 */
								c = typebuf[typeoff] & 255;
								if (advance)	/* remove chars from typebuf */
								{
									if (typemaplen)
										KeyTyped = FALSE;
									else
									{
										KeyTyped = TRUE;
										/* write char to script file(s) */
										gotchars(typebuf + typeoff, 1);
									}
									del_typebuf(1, 0);
								}
								break;		/* got character, break for loop */
							}
						}
						if (keylen > 0)		/* full matching terminal code */
						{
#ifdef USE_GUI
							if (typebuf[typeoff] == K_SPECIAL &&
											  typebuf[typeoff + 1] == KS_MENU)
							{
								/*
								 * Using a menu causes a break in undo!
								 */
								u_sync();
								del_typebuf(3, 0);
								idx = gui_get_menu_index(current_menu,
																 local_State);
								if (idx != MENU_INDEX_INVALID)
								{
									ins_typebuf(current_menu->strings[idx],
										current_menu->noremap[idx] ? -1 : 0,
										0, TRUE);
								}
							}
#endif /* USE_GUI */
							continue;	/* try mapping again */
						}

						/* partial match: get some more characters */
						keylen = K_NEEDMORET;
					}
						/* complete match */
					if (keylen >= 0 && keylen <= typelen)
					{
										/* write chars to script file(s) */
						if (keylen > typemaplen)
							gotchars(typebuf + typeoff + typemaplen,
														keylen - typemaplen);

						del_typebuf(keylen, 0);	/* remove the mapped keys */

						/*
						 * Put the replacement string in front of mapstr.
						 * The depth check catches ":map x y" and ":map y x".
						 */
						if (++mapdepth >= p_mmd)
						{
							EMSG("recursive mapping");
							if (State == CMDLINE)
								redrawcmdline();
							else
								setcursor();
							flush_buffers(FALSE);
							mapdepth = 0;		/* for next one */
							c = -1;
							break;
						}
						/*
						 * Insert the 'to' part in the typebuf.
						 * If 'from' field is the same as the start of the
						 * 'to' field, don't remap this part.
						 * If m_noremap is set, don't remap the whole 'to'
						 * part.
						 */
						if (ins_typebuf(mp->m_str, mp->m_noremap ? -1 :
												  STRNCMP(mp->m_str, mp->m_keys,
												   (size_t)keylen) ? 0 : keylen,
															   0, TRUE) == FAIL)
						{
							c = -1;
							break;
						}
						continue;
					}
				}
				/*
				 * special case: if we get an <ESC> in insert mode and there
				 * are no more characters at once, we pretend to go out of
				 * insert mode.  This prevents the one second delay after
				 * typing an <ESC>.  If we get something after all, we may
				 * have to redisplay the mode. That the cursor is in the wrong
				 * place does not matter.
				 */
				c = 0;
				if (advance && typelen == 1 && typebuf[typeoff] == ESC &&
						 !no_mapping && typemaplen == 0 && (State & INSERT) &&
					   (p_timeout || (keylen == K_NEEDMORET && p_ttimeout)) &&
						(c = inchar(typebuf + typeoff + typelen, 3, 0L)) == 0)
				{
					if (p_smd)
					{
						delmode();
						mode_deleted = TRUE;
					}
					if (curwin->w_cursor.col != 0)	/* move cursor one left if
														possible */
					{
						if (curwin->w_col)
						{
							if (did_ai)
							{
								/*
								 * We are expecting to truncate the trailing
								 * white-space, so find the last non-white
								 * character -- webb
								 */
								colnr_t		col, vcol;
								char_u		*ptr;

								col = vcol = curwin->w_col = 0;
								ptr = ml_get_curline();
								while (col < curwin->w_cursor.col)
								{
									if (!vim_iswhite(ptr[col]))
										curwin->w_col = vcol;
									vcol += lbr_chartabsize(ptr + col,
															   (colnr_t)vcol);
									++col;
								}
  								if (curwin->w_p_nu)
									curwin->w_col += 8;
							}
							else
								--curwin->w_col;
						}
						else if (curwin->w_p_wrap && curwin->w_row)
						{
								--curwin->w_row;
								curwin->w_col = Columns - 1;
						}
					}
					setcursor();
					flushbuf();
				}
				typelen += c;
													/* buffer full, don't map */
				if (typelen >= typemaplen + MAXMAPLEN)
				{
					timedout = TRUE;
					continue;
				}
/*
 * get a character: 4. from the user
 */
				/*
				 * If we have a partial match (and are going to wait for more
				 * input from the user), show the partially matched characters
				 * to the user with showcmd -- webb.
				 */
				i = 0;
				if (typelen > 0 && (State & (NORMAL | INSERT)) && advance)
				{
					push_showcmd();
					while (i < typelen)
						(void)add_to_showcmd(typebuf[typeoff + i++], TRUE);
				}

				c = inchar(typebuf + typeoff + typelen,
						typemaplen + MAXMAPLEN - typelen,
						!advance
							? 0
							: ((typelen == 0 || !(p_timeout || (p_ttimeout &&
										keylen == K_NEEDMORET)))
									? -1L
									: ((keylen == K_NEEDMORET && p_ttm >= 0)
											? p_ttm
											: p_tm)));

				if (i)
					pop_showcmd();

				if (c <= NUL)		/* no character available */
				{
					if (!advance)
						break;
					if (typelen)				/* timed out */
					{
						timedout = TRUE;
						continue;
					}
				}
				else
				{			/* allow mapping for just typed characters */
					while (typebuf[typeoff + typelen] != NUL)
						noremapbuf[typeoff + typelen++] = FALSE;
				}
			}		/* for (;;) */
		}		/* if (!character from stuffbuf) */

						/* if advance is FALSE don't loop on NULs */
	} while (c < 0 || (advance && c == NUL));

	/*
	 * The "INSERT" message is taken care of here:
	 *   if we return an ESC to exit insert mode, the message is deleted
	 *   if we don't return an ESC but deleted the message before, redisplay it
	 */
	if (p_smd && (State & INSERT))
	{
		if (c == ESC && !mode_deleted && !no_mapping)
			delmode();
		else if (c != ESC && mode_deleted)
			showmode();
	}

	return c;
}

/*
 * inchar() - get one character from
 *		1. a scriptfile
 *		2. the keyboard
 *
 *  As much characters as we can get (upto 'maxlen') are put in buf and
 *  NUL terminated (buffer length must be 'maxlen' + 1).
 *  Minimum for 'maxlen' is 3!!!!
 *
 *	If we got an interrupt all input is read until none is available.
 *
 *  If wait_time == 0  there is no waiting for the char.
 *  If wait_time == n  we wait for n msec for a character to arrive.
 *  If wait_time == -1 we wait forever for a character to arrive.
 *
 *  Return the number of obtained characters.
 */

	static int
inchar(buf, maxlen, wait_time)
	char_u	*buf;
	int		maxlen;
	long	wait_time;						/* milli seconds */
{
	int				len = 0;			/* init for GCC */
	int				retesc = FALSE;		/* return ESC with gotint */
	register int 	c;
	register int	i;

	if (wait_time == -1L || wait_time > 100L)  /* flush output before waiting */
	{
		cursor_on();
		flushbuf();
	}
	/*
	 * Don't reset these when at the hit-return prompt, otherwise a endless
	 * recursive loop may result (write error in swapfile, hit-return, timeout
	 * on char wait, flush swapfile, write error....).
	 */
	if (State != HITRETURN)
	{
		did_outofmem_msg = FALSE;	/* display out of memory message (again) */
		did_swapwrite_msg = FALSE;	/* display swap file write error again */
	}
	undo_off = FALSE;			/* restart undo now */

/*
 * first try script file
 *	If interrupted: Stop reading script files.
 */
	c = -1;
	while (scriptin[curscript] != NULL && c < 0)
	{
		if (got_int || (c = getc(scriptin[curscript])) < 0)	/* reached EOF */
		{
				/* when reading script file is interrupted, return an ESC to
									get back to normal mode */
			if (got_int)
				retesc = TRUE;
			fclose(scriptin[curscript]);
			scriptin[curscript] = NULL;
			if (curscript > 0)
				--curscript;
		}
		else
		{
			buf[0] = c;
			len = 1;
		}
	}

	if (c < 0)			/* did not get a character from script */
	{
	/*
	 * If we got an interrupt, skip all previously typed characters and
	 * return TRUE if quit reading script file.
	 */
		if (got_int)			/* skip typed characters */
		{
			while (mch_inchar(buf, maxlen, 0L))
				;
			return retesc;
		}
			/* fill up to a third of the buffer, because each character may be
			 * tripled below */
		len = mch_inchar(buf, maxlen / 3, wait_time);
	}

	/*
	 * Two characters are special: NUL and K_SPECIAL.
	 * Replace       NUL by K_SPECIAL KS_ZERO	 K_FILLER
	 * Replace K_SPECIAL by K_SPECIAL KS_SPECIAL K_FILLER
	 * Don't replace K_SPECIAL when reading a script file.
	 */
	for (i = len; --i >= 0; ++buf)
	{
		if (buf[0] == NUL || (buf[0] == K_SPECIAL && c < 0))
		{
			vim_memmove(buf + 3, buf + 1, (size_t)i);
			buf[2] = K_THIRD(buf[0]);
			buf[1] = K_SECOND(buf[0]);
			buf[0] = K_SPECIAL;
			buf += 2;
			len += 2;
		}
	}
	*buf = NUL;								/* add trailing NUL */
	return len;
}

/*
 * map[!]					: show all key mappings
 * map[!] {lhs}				: show key mapping for {lhs}
 * map[!] {lhs} {rhs}		: set key mapping for {lhs} to {rhs}
 * noremap[!] {lhs} {rhs}	: same, but no remapping for {rhs}
 * unmap[!] {lhs}			: remove key mapping for {lhs}
 * abbr						: show all abbreviations
 * abbr {lhs}				: show abbreviations for {lhs}
 * abbr {lhs} {rhs}			: set abbreviation for {lhs} to {rhs}
 * noreabbr {lhs} {rhs}		: same, but no remapping for {rhs}
 * unabbr {lhs}				: remove abbreviation for {lhs}
 *
 * maptype == 1 for unmap command, 2 for noremap command.
 *
 * keys is pointer to any arguments. Note: keys cannot be a read-only string,
 * it will be modified.
 *
 * for :map	  mode is NORMAL + VISUAL
 * for :map!  mode is INSERT + CMDLINE
 * for :cmap  mode is CMDLINE
 * for :imap  mode is INSERT 
 * for :nmap  mode is NORMAL
 * for :vmap  mode is VISUAL
 * for :abbr  mode is INSERT + CMDLINE + ABBREV
 * for :iabbr mode is INSERT + ABBREV
 * for :cabbr mode is CMDLINE + ABBREV
 * 
 * Return 0 for success
 *		  1 for invalid arguments
 *		  2 for no match
 *		  3 for ambiguety
 *		  4 for out of mem
 */
	int
do_map(maptype, keys, mode)
	int		maptype;
	char_u	*keys;
	int		mode;
{
	struct mapblock		*mp, *mprev;
	char_u				*arg;
	char_u				*p;
	int					n;
	int					len = 0;		/* init for GCC */
	char_u				*newstr;
	int					hasarg;
	int					haskey;
	int					did_it = FALSE;
	int					abbrev = 0;
	int					round;
	char_u				*keys_buf = NULL;
	char_u				*arg_buf = NULL;
	int					retval = 0;
	int					do_backslash;

	if (mode & ABBREV)		/* not a mapping but an abbreviation */
	{
		abbrev = ABBREV;
		mode &= ~ABBREV;
	}
/*
 * find end of keys and skip CTRL-Vs (and backslashes) in it
 * Accept backslash like CTRL-V when 'cpoptions' does not contain 'B'.
 * with :unmap white space is included in the keys, no argument possible
 */
	p = keys;
	do_backslash = (vim_strchr(p_cpo, CPO_BSLASH) == NULL);
	while (*p && (maptype == 1 || !vim_iswhite(*p)))
	{
		if ((p[0] == Ctrl('V') || (do_backslash && p[0] == '\\')) &&
																  p[1] != NUL)
			++p;				/* skip CTRL-V or backslash */
		++p;
	}
	if (*p != NUL)
		*p++ = NUL;
	p = skipwhite(p);
	arg = p;
	hasarg = (*arg != NUL);
	haskey = (*keys != NUL);

		/* check for :unmap without argument */
	if (maptype == 1 && !haskey)	
	{
		retval = 1;
		goto theend;
	}

	/*
	 * If mapping has been given as ^V<C_UP> say, then replace the term codes
	 * with the appropriate two bytes. If it is a shifted special key, unshift
	 * it too, giving another two bytes.
	 * replace_termcodes() may move the result to allocated memory, which
	 * needs to be freed later (*keys_buf and *arg_buf).
	 * replace_termcodes() also removes CTRL-Vs and sometimes backslashes.
	 */
	if (haskey)
		keys = replace_termcodes(keys, &keys_buf, TRUE);
	if (hasarg)
		arg = replace_termcodes(arg, &arg_buf, FALSE);

/*
 * check arguments and translate function keys
 */
	if (haskey)
	{
		len = STRLEN(keys);
		if (len > MAXMAPLEN)			/* maximum length of MAXMAPLEN chars */
		{
			retval = 1;
			goto theend;
		}

		if (abbrev)
		{
			/*
			 * If an abbreviation ends in a keyword character, the
			 * rest must be all keyword-char or all non-keyword-char.
			 * Otherwise we won't be able to find the start of it in a
			 * vi-compatible way.
			 * An abbrevation cannot contain white space.
			 */
			if (iswordchar(keys[len - 1]))  	/* ends in keyword char */
				for (n = 0; n < len - 2; ++n)
					if (iswordchar(keys[n]) != iswordchar(keys[len - 2]))
					{
						retval = 1;
						goto theend;
					}
			for (n = 0; n < len; ++n)
				if (vim_iswhite(keys[n]))
				{
					retval = 1;
					goto theend;
				}
		}
	}

	if (haskey && hasarg && abbrev)		/* if we will add an abbreviation */
		no_abbr = FALSE;				/* reset flag that indicates there are
															no abbreviations */

	if (!haskey || (maptype != 1 && !hasarg))
		msg_start();
/*
 * Find an entry in the maplist that matches.
 * For :unmap we may loop two times: once to try to unmap an entry with a
 * matching 'from' part, a second time, if the first fails, to unmap an
 * entry with a matching 'to' part. This was done to allow ":ab foo bar" to be
 * unmapped by typing ":unab foo", where "foo" will be replaced by "bar" because
 * of the abbreviation.
 */
	for (round = 0; (round == 0 || maptype == 1) && round <= 1 &&
												 !did_it && !got_int; ++round)
	{
		for (mp = maplist.m_next, mprev = &maplist; mp && !got_int;
												  mprev = mp, mp = mp->m_next)
		{
										/* skip entries with wrong mode */
			if (!(mp->m_mode & mode) || (mp->m_mode & ABBREV) != abbrev)
				continue;
			if (!haskey)						/* show all entries */
			{
				showmap(mp);
				did_it = TRUE;
			}
			else								/* do we have a match? */
			{
				if (round)		/* second round: try 'to' string for unmap */
				{
					n = STRLEN(mp->m_str);
					p = mp->m_str;
				}
				else
				{
					n = mp->m_keylen;
					p = mp->m_keys;
				}
				if (!STRNCMP(p, keys, (size_t)(n < len ? n : len)))
				{
					if (maptype == 1)			/* delete entry */
					{
						if (n != len)			/* not a full match */
							continue;
						/*
						 * We reset the indicated mode bits. If nothing is
						 * left the entry is deleted below.
						 */
						mp->m_mode &= (~mode | ABBREV);
						did_it = TRUE;			/* remember that we did something */
					}
					else if (!hasarg)			/* show matching entry */
					{
						showmap(mp);
						did_it = TRUE;
					}
					else if (n != len)			/* new entry is ambigious */
					{
						if (abbrev)				/* for abbreviations that's ok */
							continue;
						retval = 3;
						goto theend;
					}
					else
					{
						mp->m_mode &= (~mode | ABBREV);		/* remove mode bits */
						if (!(mp->m_mode & ~ABBREV) && !did_it)	/* reuse existing entry */
						{
							newstr = strsave(arg);
							if (newstr == NULL)
							{
								retval = 4;			/* no mem */
								goto theend;
							}
							vim_free(mp->m_str);
							mp->m_str = newstr;
							mp->m_noremap = maptype;
							mp->m_mode = mode + abbrev;
							did_it = TRUE;
						}
					}
					if (!(mp->m_mode & ~ABBREV))	/* entry can be deleted */
					{
						map_free(mprev);
						mp = mprev;				/* continue with next entry */
					}
				}
			}
		}
	}

	if (maptype == 1)						/* delete entry */
	{
		if (!did_it)
			retval = 2;						/* no match */
		goto theend;
	}

	if (!haskey || !hasarg)					/* print entries */
	{
		if (!did_it)
		{
			if (abbrev)
				MSG("No abbreviation found");
			else
				MSG("No mapping found");
		}
		goto theend;						/* listing finished */
	}

	if (did_it)					/* have added the new entry already */
		goto theend;
/*
 * get here when we have to add a new entry
 */
		/* allocate a new entry for the maplist */
	mp = (struct mapblock *)alloc((unsigned)sizeof(struct mapblock));
	if (mp == NULL)
	{
		retval = 4;			/* no mem */
		goto theend;
	}
	mp->m_keys = strsave(keys);
	mp->m_str = strsave(arg);
	if (mp->m_keys == NULL || mp->m_str == NULL)
	{
		vim_free(mp->m_keys);
		vim_free(mp->m_str);
		vim_free(mp);
		retval = 4;		/* no mem */
		goto theend;
	}
	mp->m_keylen = STRLEN(mp->m_keys);
	mp->m_noremap = maptype;
	mp->m_mode = mode + abbrev;

	/* add the new entry in front of the maplist */
	mp->m_next = maplist.m_next;
	maplist.m_next = mp;

theend:
	vim_free(keys_buf);
	vim_free(arg_buf);
	return retval;
}

/*
 * Delete one entry from the maplist.
 * The argument is a pointer to the PREVIOUS entry!
 */
	static void
map_free(mprev)
	struct mapblock		*mprev;
{
	struct mapblock		*mp;
	
	mp = mprev->m_next;
	vim_free(mp->m_keys);
	vim_free(mp->m_str);
	mprev->m_next = mp->m_next;
	vim_free(mp);
}

/*
 * Clear all mappings or abbreviations.
 * 'abbr' should be FALSE for mappings, TRUE for abbreviations.
 */
	void
map_clear(modec, force, abbr)
	int		modec;
	int		force;
	int		abbr;
{
	struct mapblock		*mp;
	int		mode;

	if (force)			/* :mapclear! */
		mode = INSERT + CMDLINE;
	else if (modec == 'i')
		mode = INSERT;
	else if (modec == 'n')
		mode = NORMAL;
	else if (modec == 'c')
		mode = CMDLINE;
	else if (modec == 'v')
		mode = VISUAL;
	else
		mode = VISUAL + NORMAL;

	for (mp = &maplist; mp->m_next != NULL; )
	{
		if (abbr != !(mp->m_next->m_mode & ABBREV) && mp->m_next->m_mode & mode)
		{
			mp->m_next->m_mode &= ~mode;
			if ((mp->m_next->m_mode & ~ABBREV) == 0) /* entry can be deleted */
			{
				map_free(mp);
				continue;
			}
		}
		mp = mp->m_next;
	}
}

	static void
showmap(mp)
	struct mapblock *mp;
{
	int len;

	if (msg_didout)
		msg_outchar('\n');
	if ((mp->m_mode & (INSERT + CMDLINE)) == INSERT + CMDLINE)
		MSG_OUTSTR("! ");
	else if (mp->m_mode & INSERT)
		MSG_OUTSTR("i ");
	else if (mp->m_mode & CMDLINE)
		MSG_OUTSTR("c ");
	else if (!(mp->m_mode & VISUAL))
		MSG_OUTSTR("n ");
	else if (!(mp->m_mode & NORMAL))
		MSG_OUTSTR("v ");
	else
		MSG_OUTSTR("  ");
	/* Get length of what we write */
	len = msg_outtrans_special(mp->m_keys, TRUE);
	do
	{
		msg_outchar(' ');				/* padd with blanks */
		++len;
	} while (len < 12);
	if (mp->m_noremap)
		msg_outchar('*');
	else
		msg_outchar(' ');
	/* Use FALSE below if we only want things like <Up> to show up as such on
	 * the rhs, and not M-x etc, TRUE gets both -- webb
	 */
	msg_outtrans_special(mp->m_str, TRUE);
	flushbuf();							/* show one line at a time */
}

/*
 * Check for an abbreviation.
 * Cursor is at ptr[col]. When inserting, mincol is where insert started.
 * "c" is the character typed before check_abbr was called.
 *
 * Historic vi practice: The last character of an abbreviation must be an id
 * character ([a-zA-Z0-9_]). The characters in front of it must be all id
 * characters or all non-id characters. This allows for abbr. "#i" to
 * "#include".
 *
 * Vim addition: Allow for abbreviations that end in a non-keyword character.
 * Then there must be white space before the abbr.
 *
 * return TRUE if there is an abbreviation, FALSE if not
 */
	int
check_abbr(c, ptr, col, mincol)
	int		c;
	char_u	*ptr;
	int		col;
	int		mincol;
{
	int				len;
	int				j;
	char_u			tb[4];
	struct mapblock *mp;
	int				is_id = TRUE;
	int				vim_abbr;

	if (no_abbr_cnt)		/* abbrev. are not recursive */
		return FALSE;

	/*
	 * Check for word before the cursor: If it ends in a keyword char all
	 * chars before it must be al keyword chars or non-keyword chars, but not
	 * white space. If it ends in a non-keyword char we accept any characters
	 * before it except white space.
	 */
	if (col == 0)								/* cannot be an abbr. */
		return FALSE;

	if (!iswordchar(ptr[col - 1]))
		vim_abbr = TRUE;						/* Vim added abbr. */
	else
	{
		vim_abbr = FALSE;						/* vi compatible abbr. */
		if (col > 1)
			is_id = iswordchar(ptr[col - 2]);
	}
	for (len = col - 1; len > 0 && !vim_isspace(ptr[len - 1]) &&
					   (vim_abbr || is_id == iswordchar(ptr[len - 1])); --len)
		;

	if (len < mincol)
		len = mincol;
	if (len < col)				/* there is a word in front of the cursor */
	{
		ptr += len;
		len = col - len;
		for (mp = maplist.m_next; mp; mp = mp->m_next)
		{
					/* find entries with right mode and keys */
			if ((mp->m_mode & ABBREV) == ABBREV &&
						(mp->m_mode & State) &&
						mp->m_keylen == len &&
						!STRNCMP(mp->m_keys, ptr, (size_t)len))
				break;
		}
		if (mp)
		{
			/*
			 * Found a match:
			 * Insert the rest of the abbreviation in typebuf[].
			 * This goes from end to start.
			 *
			 * Characters 0x000 - 0x100: normal chars, may need CTRL-V,
			 * except K_SPECIAL: Becomes K_SPECIAL KS_SPECIAL K_FILLER
			 * Characters where IS_SPECIAL() == TRUE: key codes, need
			 * K_SPECIAL. Other characters (with ABBR_OFF): don't use CTRL-V.
			 */
			j = 0;
											/* special key code, split up */
			if (IS_SPECIAL(c) || c == K_SPECIAL)
			{
				tb[j++] = K_SPECIAL;
				tb[j++] = K_SECOND(c);
				c = K_THIRD(c);
			}
			else if (c < 0x100  && (c < ' ' || c > '~'))
				tb[j++] = Ctrl('V');		/* special char needs CTRL-V */
			tb[j++] = c;
			tb[j] = NUL;
											/* insert the last typed char */
			(void)ins_typebuf(tb, TRUE, 0, TRUE);
											/* insert the to string */
			(void)ins_typebuf(mp->m_str, mp->m_noremap, 0, TRUE);
											/* no abbrev. for these chars */
			no_abbr_cnt += STRLEN(mp->m_str) + j + 1;

			tb[0] = Ctrl('H');
			tb[1] = NUL;
			while (len--)					/* delete the from string */
				(void)ins_typebuf(tb, TRUE, 0, TRUE);
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * Write map commands for the current mappings to an .exrc file.
 * Return FAIL on error, OK otherwise.
 */
	int
makemap(fd)
	FILE *fd;
{
	struct mapblock *mp;
	char_u			c1;
	char_u 			*p;

	for (mp = maplist.m_next; mp; mp = mp->m_next)
	{
		c1 = NUL;
		p = (char_u *)"map";
		switch (mp->m_mode)
		{
		case NORMAL + VISUAL:
			break;
		case NORMAL:
			c1 = 'n';
			break;
		case VISUAL:
			c1 = 'v';
			break;
		case CMDLINE + INSERT:
			p = (char_u *)"map!";
			break;
		case CMDLINE:
			c1 = 'c';
			break;
		case INSERT:
			c1 = 'i';
			break;
		case INSERT + CMDLINE + ABBREV:
			p = (char_u *)"abbr";
			break;
		case CMDLINE + ABBREV:
			c1 = 'c';
			p = (char_u *)"abbr";
			break;
		case INSERT + ABBREV:
			c1 = 'i';
			p = (char_u *)"abbr";
			break;
		default:
			EMSG("makemap: Illegal mode");
			return FAIL;
		}
		if (c1 && putc(c1, fd) < 0)
			return FAIL;
		if (mp->m_noremap && fprintf(fd, "nore") < 0)
			return FAIL;
		if (fprintf(fd, (char *)p) < 0)
			return FAIL;

		if (	putc(' ', fd) < 0 || putescstr(fd, mp->m_keys, FALSE) == FAIL ||
				putc(' ', fd) < 0 || putescstr(fd, mp->m_str, FALSE) == FAIL ||
#ifdef USE_CRNL
				putc('\r', fd) < 0 ||
#endif
				putc('\n', fd) < 0)
			return FAIL;
	}
	return OK;
}

/*
 * write escape string to file
 *
 * return FAIL for failure, OK otherwise
 */
	int
putescstr(fd, str, set)
	FILE		*fd;
	char_u		*str;
	int			set;		/* TRUE for makeset, FALSE for makemap */
{
	int		c;
	int		modifiers;

	for ( ; *str; ++str)
	{
		c = *str;
		/*
		 * Special key codes have to be translated to be able to make sense
		 * when they are read back.
		 */
		if (c == K_SPECIAL && !set)
		{
			modifiers = 0x0;
			if (str[1] == KS_MODIFIER)
			{
				modifiers = str[2];
				str += 3;
				c = *str;
			}
			if (c == K_SPECIAL)
			{
				c = TO_SPECIAL(str[1], str[2]);
				str += 2;
			}
			if (IS_SPECIAL(c) || modifiers)		/* special key */
			{
				fprintf(fd, (char *)get_special_key_name(c, modifiers));
				continue;
			}
		}
		/*
		 * A '\n' in a map command should be written as <NL>.
		 * A '\n' in a set command should be written as \^V^J.
		 */
		if (c == NL)
		{
			if (set)
				fprintf(fd, "\\\026\n");
			else
				fprintf(fd, "<NL>");
			continue;
		}
		/*
		 * some characters have to be escaped with CTRL-V to
		 * prevent them from misinterpreted in DoOneCmd().
		 * A space, Tab and '"' has to be escaped with a backslash to
		 * prevent it to be misinterpreted in do_set().
		 */
		if (set && (vim_iswhite(c) || c == '"' || c == '\\'))
		{
			if (putc('\\', fd) < 0)
				return FAIL;
		}
		else if (c < ' ' || c > '~' || c == '|')
		{
			if (putc(Ctrl('V'), fd) < 0)
				return FAIL;
		}
		if (putc(c, fd) < 0)
			return FAIL;
	}
	return OK;
}

/*
 * Check all mappings for the presence of special key codes.
 * Used after ":set term=xxx".
 */
	void
check_map_keycodes()
{
	struct mapblock *mp;
	char_u 			*p;
	int				i;
	char_u			buf[3];
	char_u			*save_name;

	save_name = sourcing_name;
	sourcing_name = (char_u *)"mappings";/* don't give error messages */
	for (mp = maplist.m_next; mp != NULL; mp = mp->m_next)
	{
		for (i = 0; i <= 1; ++i)		/* do this twice */
		{
			if (i == 0)
				p = mp->m_keys;			/* once for the "from" part */
			else
				p = mp->m_str;			/* and once for the "to" part */
			while (*p)
			{
				if (*p == K_SPECIAL)
				{
					++p;
					if (*p < 128)		/* only for "normal" termcap entries */
					{
						buf[0] = p[0];
						buf[1] = p[1];
						buf[2] = NUL;
						(void)add_termcap_entry(buf, FALSE);
					}
					++p;
				}
				++p;
			}
		}
	}
	sourcing_name = save_name;
}
