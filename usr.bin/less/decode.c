/*
 * Copyright (c) 1984,1985,1989,1994,1995  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Routines to decode user commands.
 *
 * This is all table driven.
 * A command table is a sequence of command descriptors.
 * Each command descriptor is a sequence of bytes with the following format:
 *	<c1><c2>...<cN><0><action>
 * The characters c1,c2,...,cN are the command string; that is,
 * the characters which the user must type.
 * It is terminated by a null <0> byte.
 * The byte after the null byte is the action code associated
 * with the command string.
 * If an action byte is OR-ed with A_EXTRA, this indicates
 * that the option byte is followed by an extra string.
 *
 * There may be many command tables.
 * The first (default) table is built-in.
 * Other tables are read in from "lesskey" files.
 * All the tables are linked together and are searched in order.
 */

#include "less.h"
#include "cmd.h"
#include "lesskey.h"

extern int erase_char, kill_char;

/*
 * Command table is ordered roughly according to expected
 * frequency of use, so the common commands are near the beginning.
 */
static unsigned char cmdtable[] =
{
	'\r',0,				A_F_LINE,
	'\n',0,				A_F_LINE,
	'e',0,				A_F_LINE,
	'j',0,				A_F_LINE,
	CONTROL('E'),0,			A_F_LINE,
	CONTROL('N'),0,			A_F_LINE,
	'k',0,				A_B_LINE,
	'y',0,				A_B_LINE,
	CONTROL('Y'),0,			A_B_LINE,
	CONTROL('K'),0,			A_B_LINE,
	CONTROL('P'),0,			A_B_LINE,
	'J',0,				A_FF_LINE,
	'K',0,				A_BF_LINE,
	'Y',0,				A_BF_LINE,
	'd',0,				A_F_SCROLL,
	CONTROL('D'),0,			A_F_SCROLL,
	'u',0,				A_B_SCROLL,
	CONTROL('U'),0,			A_B_SCROLL,
	' ',0,				A_F_SCREEN,
	'f',0,				A_F_SCREEN,
	CONTROL('F'),0,			A_F_SCREEN,
	CONTROL('V'),0,			A_F_SCREEN,
	'b',0,				A_B_SCREEN,
	CONTROL('B'),0,			A_B_SCREEN,
	ESC,'v',0,			A_B_SCREEN,
	'z',0,				A_F_WINDOW,
	'w',0,				A_B_WINDOW,
	'F',0,				A_F_FOREVER,
	'R',0,				A_FREPAINT,
	'r',0,				A_REPAINT,
	CONTROL('R'),0,			A_REPAINT,
	CONTROL('L'),0,			A_REPAINT,
	ESC,'u',0,			A_UNDO_SEARCH,
	'g',0,				A_GOLINE,
	'<',0,				A_GOLINE,
	ESC,'<',0,			A_GOLINE,
	'p',0,				A_PERCENT,
	'%',0,				A_PERCENT,
	'{',0,				A_F_BRACKET|A_EXTRA,	'{','}',0,
	'}',0,				A_B_BRACKET|A_EXTRA,	'{','}',0,
	'(',0,				A_F_BRACKET|A_EXTRA,	'(',')',0,
	')',0,				A_B_BRACKET|A_EXTRA,	'(',')',0,
	'[',0,				A_F_BRACKET|A_EXTRA,	'[',']',0,
	']',0,				A_B_BRACKET|A_EXTRA,	'[',']',0,
	ESC,CONTROL('F'),0,		A_F_BRACKET,
	ESC,CONTROL('B'),0,		A_B_BRACKET,
	'G',0,				A_GOEND,
	ESC,'>',0,			A_GOEND,
	'>',0,				A_GOEND,
	'P',0,				A_GOPOS,

	'0',0,				A_DIGIT,
	'1',0,				A_DIGIT,
	'2',0,				A_DIGIT,
	'3',0,				A_DIGIT,
	'4',0,				A_DIGIT,
	'5',0,				A_DIGIT,
	'6',0,				A_DIGIT,
	'7',0,				A_DIGIT,
	'8',0,				A_DIGIT,
	'9',0,				A_DIGIT,

	'=',0,				A_STAT,
	CONTROL('G'),0,			A_STAT,
	':','f',0,			A_STAT,
	'/',0,				A_F_SEARCH,
	'?',0,				A_B_SEARCH,
	ESC,'/',0,			A_F_SEARCH|A_EXTRA,	'*',0,
	ESC,'?',0,			A_B_SEARCH|A_EXTRA,	'*',0,
	'n',0,				A_AGAIN_SEARCH,
	ESC,'n',0,			A_T_AGAIN_SEARCH,
	'N',0,				A_REVERSE_SEARCH,
	ESC,'N',0,			A_T_REVERSE_SEARCH,
	'm',0,				A_SETMARK,
	'\'',0,				A_GOMARK,
	CONTROL('X'),CONTROL('X'),0,	A_GOMARK,
	'E',0,				A_EXAMINE,
	':','e',0,			A_EXAMINE,
	CONTROL('X'),CONTROL('V'),0,	A_EXAMINE,
	':','n',0,			A_NEXT_FILE,
	':','p',0,			A_PREV_FILE,
	':','x',0,			A_INDEX_FILE,
	'-',0,				A_OPT_TOGGLE,
	':','t',0,			A_OPT_TOGGLE|A_EXTRA,	't',0,
	's',0,				A_OPT_TOGGLE|A_EXTRA,	'o',0,
	'_',0,				A_DISP_OPTION,
	'|',0,				A_PIPE,
	'v',0,				A_VISUAL,
	'!',0,				A_SHELL,
	'+',0,				A_FIRSTCMD,

	'H',0,				A_HELP,
	'h',0,				A_HELP,
	'V',0,				A_VERSION,
	'q',0,				A_QUIT,
	':','q',0,			A_QUIT,
	':','Q',0,			A_QUIT,
	'Z','Z',0,			A_QUIT
};

static unsigned char edittable[] =
{
	'\t',0,	    		EC_F_COMPLETE,	/* TAB */
	'\17',0,		EC_B_COMPLETE,	/* BACKTAB */
	'\14',0,		EC_EXPAND,	/* CTRL-L */
	CONTROL('V'),0,		EC_LITERAL,	/* BACKSLASH */
	CONTROL('A'),0,		EC_LITERAL,	/* BACKSLASH */
   	ESC,'l',0,		EC_RIGHT,	/* ESC l */
	ESC,'h',0,		EC_LEFT,	/* ESC h */
	ESC,'b',0,		EC_W_LEFT,	/* ESC b */
	ESC,'w',0,		EC_W_RIGHT,	/* ESC w */
	ESC,'i',0,		EC_INSERT,	/* ESC i */
	ESC,'x',0,		EC_DELETE,	/* ESC x */
	ESC,'X',0,		EC_W_DELETE,	/* ESC X */
	ESC,'\b',0,		EC_W_BACKSPACE,	/* ESC BACKSPACE */
	ESC,'0',0,		EC_HOME,	/* ESC 0 */
	ESC,'$',0,		EC_END,		/* ESC $ */
	ESC,'k',0,		EC_UP,		/* ESC k */
	ESC,'j',0,		EC_DOWN,	/* ESC j */
	ESC,'\t',0,		EC_B_COMPLETE	/* ESC TAB */
};

/*
 * Structure to support a list of command tables.
 */
struct tablelist
{
	struct tablelist *t_next;
	char *t_start;
	char *t_end;
};

/*
 * List of command tables and list of line-edit tables.
 */
static struct tablelist *list_fcmd_tables = NULL;
static struct tablelist *list_ecmd_tables = NULL;


/*
 * Initialize the command lists.
 */
	public void
init_cmds()
{
	/*
	 * Add the default command tables.
	 */
	add_fcmd_table((char*)cmdtable, sizeof(cmdtable));
	add_ecmd_table((char*)edittable, sizeof(edittable));
	get_editkeys();
#if USERFILE
	/*
	 * Try to add the tables in the standard lesskey file "$HOME/.less".
	 */
	add_hometable();
#endif
}

/*
 * 
 */
	static int
add_cmd_table(tlist, buf, len)
	struct tablelist **tlist;
	char *buf;
	int len;
{
	register struct tablelist *t;

	if (len == 0)
		return (0);
	/*
	 * Allocate a tablelist structure, initialize it, 
	 * and link it into the list of tables.
	 */
	if ((t = (struct tablelist *) 
			calloc(1, sizeof(struct tablelist))) == NULL)
	{
		return (-1);
	}
	t->t_start = buf;
	t->t_end = buf + len;
	t->t_next = *tlist;
	*tlist = t;
	return (0);
}

/*
 * Add a command table.
 */
	public void
add_fcmd_table(buf, len)
	char *buf;
	int len;
{
	if (add_cmd_table(&list_fcmd_tables, buf, len) < 0)
		error("Warning: some commands disabled", NULL_PARG);
}

/*
 * Add an editing command table.
 */
	public void
add_ecmd_table(buf, len)
	char *buf;
	int len;
{
	if (add_cmd_table(&list_ecmd_tables, buf, len) < 0)
		error("Warning: some edit commands disabled", NULL_PARG);
}

/*
 * Search a single command table for the command string in cmd.
 */
	public int
cmd_search(cmd, table, endtable, sp)
	char *cmd;
	char *table;
	char *endtable;
	char **sp;
{
	register char *p;
	register char *q;
	register int a;

	for (p = table, q = cmd;  p < endtable;  p++, q++)
	{
		if (*p == *q)
		{
			/*
			 * Current characters match.
			 * If we're at the end of the string, we've found it.
			 * Return the action code, which is the character
			 * after the null at the end of the string
			 * in the command table.
			 */
			if (*p == '\0')
			{
				a = *++p & 0377;
				if (a == A_END_LIST)
				{
					/*
					 * We get here only if the original
					 * cmd string passed in was empty ("").
					 * I don't think that can happen,
					 * but just in case ...
					 */
					return (A_UINVALID);
				}
				/*
				 * Check for an "extra" string.
				 */
				if (a & A_EXTRA)
				{
					*sp = ++p;
					a &= ~A_EXTRA;
				} else
					*sp = NULL;
				return (a);
			}
		} else if (*q == '\0')
		{
			/*
			 * Hit the end of the user's command,
			 * but not the end of the string in the command table.
			 * The user's command is incomplete.
			 */
			return (A_PREFIX);
		} else
		{
			/*
			 * Not a match.
			 * Skip ahead to the next command in the
			 * command table, and reset the pointer
			 * to the beginning of the user's command.
			 */
			if (*p == '\0' && p[1] == A_END_LIST)
			{
				/*
				 * A_END_LIST is a special marker that tells 
				 * us to abort the cmd search.
				 */
				return (A_UINVALID);
			}
			while (*p++ != '\0') ;
			if (*p & A_EXTRA)
				while (*++p != '\0') ;
			q = cmd-1;
		}
	}
	/*
	 * No match found in the entire command table.
	 */
	return (A_INVALID);
}

/*
 * Decode a command character and return the associated action.
 * The "extra" string, if any, is returned in sp.
 */
	static int
cmd_decode(tlist, cmd, sp)
	struct tablelist *tlist;
	char *cmd;
	char **sp;
{
	register struct tablelist *t;
	register int action = A_INVALID;

	/*
	 * Search thru all the command tables.
	 * Stop when we find an action which is not A_INVALID.
	 */
	for (t = tlist;  t != NULL;  t = t->t_next)
	{
		action = cmd_search(cmd, t->t_start, t->t_end, sp);
		if (action != A_INVALID)
			break;
	}
	return (action);
}

/*
 * Decode a command from the cmdtables list.
 */
	public int
fcmd_decode(cmd, sp)
	char *cmd;
	char **sp;
{
	return (cmd_decode(list_fcmd_tables, cmd, sp));
}

/*
 * Decode a command from the edittables list.
 */
	public int
ecmd_decode(cmd, sp)
	char *cmd;
	char **sp;
{
	return (cmd_decode(list_ecmd_tables, cmd, sp));
}

#if USERFILE
	static int
gint(sp)
	char **sp;
{
	int n;

	n = *(*sp)++;
	n += *(*sp)++ * KRADIX;
	return (n);
}

	static int
old_lesskey(buf, len)
	char *buf;
	int len;
{
	/*
	 * Old-style lesskey file.
	 * The file must end with either 
	 *     ...,cmd,0,action
	 * or  ...,cmd,0,action|A_EXTRA,string,0
	 * So the last byte or the second to last byte must be zero.
	 */
	if (buf[len-1] != '\0' && buf[len-2] != '\0')
		return (-1);
	add_fcmd_table(buf, len);
	return (0);
}

	static int
new_lesskey(buf, len)
	char *buf;
	int len;
{
	char *p;
	register int c;
	register int done;
	register int n;

	/*
	 * New-style lesskey file.
	 * Extract the pieces.
	 */
	if (buf[len-3] != C0_END_LESSKEY_MAGIC ||
	    buf[len-2] != C1_END_LESSKEY_MAGIC ||
	    buf[len-1] != C2_END_LESSKEY_MAGIC)
		return (-1);
	p = buf + 4;
	done = 0;
	while (!done)
	{
		c = *p++;
		switch (c)
		{
		case CMD_SECTION:
			n = gint(&p);
			add_fcmd_table(p, n);
			p += n;
			break;
		case EDIT_SECTION:
			n = gint(&p);
			add_ecmd_table(p, n);
			p += n;
			break;
		case END_SECTION:
			done = 1;
			break;
		default:
			free(buf);
			return (-1);
		}
	}
	return (0);
}

/*
 * Set up a user command table, based on a "lesskey" file.
 */
	public int
lesskey(filename)
	char *filename;
{
	register char *buf;
	register POSITION len;
	register long n;
	register int f;

	/*
	 * Try to open the lesskey file.
	 */
	f = open(filename, OPEN_READ);
	if (f < 0)
		return (1);

	/*
	 * Read the file into a buffer.
	 * We first figure out the size of the file and allocate space for it.
	 * {{ Minimal error checking is done here.
	 *    A garbage .less file will produce strange results.
	 *    To avoid a large amount of error checking code here, we
	 *    rely on the lesskey program to generate a good .less file. }}
	 */
	len = filesize(f);
	if (len == NULL_POSITION || len < 3)
	{
		/*
		 * Bad file (valid file must have at least 3 chars).
		 */
		close(f);
		return (-1);
	}
	if ((buf = (char *) calloc((int)len, sizeof(char))) == NULL)
	{
		close(f);
		return (-1);
	}
	if (lseek(f, (off_t)0, 0) == BAD_LSEEK)
	{
		free(buf);
		close(f);
		return (-1);
	}
	n = read(f, buf, (unsigned int) len);
	close(f);
	if (n != len)
	{
		free(buf);
		return (-1);
	}

	/*
	 * Figure out if this is an old-style (before version 241)
	 * or new-style lesskey file format.
	 */
	if (buf[0] != C0_LESSKEY_MAGIC || buf[1] != C1_LESSKEY_MAGIC ||
	    buf[2] != C2_LESSKEY_MAGIC || buf[3] != C3_LESSKEY_MAGIC)
		return (old_lesskey(buf, (int)len));
	return (new_lesskey(buf, (int)len));
}

/*
 * Add the standard lesskey file "$HOME/.less"
 */
	public void
add_hometable()
{
	char *filename;
	PARG parg;

	filename = homefile(LESSKEYFILE);
	if (filename == NULL)
		return;
	if (lesskey(filename) < 0)
	{
		parg.p_string = filename;
		error("Cannot use lesskey file \"%s\"", &parg);
	}
	free(filename);
}
#endif

/*
 * See if a char is a special line-editing command.
 */
	public int
editchar(c, flags)
	int c;
	int flags;
{
	int action;
	int nch;
	char *s;
	char usercmd[MAX_CMDLEN+1];
	
	/*
	 * An editing character could actually be a sequence of characters;
	 * for example, an escape sequence sent by pressing the uparrow key.
	 * To match the editing string, we use the command decoder
	 * but give it the edit-commands command table
	 * This table is constructed to match the user's keyboard.
	 */
	if (c == erase_char)
		return (EC_BACKSPACE);
	if (c == kill_char)
		return (EC_LINEKILL);
		
	/*
	 * Collect characters in a buffer.
	 * Start with the one we have, and get more if we need them.
	 */
	nch = 0;
	do {
	  	if (nch > 0)
			c = getcc();
		usercmd[nch] = c;
		usercmd[nch+1] = '\0';
		nch++;
		action = ecmd_decode(usercmd, &s);
	} while (action == A_PREFIX);
	
	if (flags & EC_NOHISTORY) 
	{
		/*
		 * The caller says there is no history list.
		 * Reject any history-manipulation action.
		 */
		switch (action)
		{
		case EC_UP:
		case EC_DOWN:
			action = A_INVALID;
			break;
		}
	}
	if (flags & EC_NOCOMPLETE) 
	{
		/*
		 * The caller says we don't want any filename completion cmds.
		 * Reject them.
		 */
		switch (action)
		{
		case EC_F_COMPLETE:
		case EC_B_COMPLETE:
		case EC_EXPAND:
			action = A_INVALID;
			break;
		}
	}
	if ((flags & EC_PEEK) || action == A_INVALID)
	{
		/*
		 * We're just peeking, or we didn't understand the command.
		 * Unget all the characters we read in the loop above.
		 * This does NOT include the original character that was 
		 * passed in as a parameter.
		 */
		while (nch > 1) {
			ungetcc(usercmd[--nch]);
		}
	} else
	{
		if (s != NULL)
			ungetsc(s);
	}
	return action;
}

