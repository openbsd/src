/* This is the Assembler Pre-Processor
   Copyright (C) 1987, 1990, 1991, 1992, 1994 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Modified by Allen Wirfs-Brock, Instantiations Inc 2/90 */
/* App, the assembler pre-processor.  This pre-processor strips out excess
   spaces, turns single-quoted characters into a decimal constant, and turns
   # <number> <filename> <garbage> into a .line <number>\n.file <filename>
   pair.  This needs better error-handling.  */

#include <stdio.h>
#include "as.h"			/* For BAD_CASE() only */

#if (__STDC__ != 1)
#ifndef const
#define const  /* empty */
#endif
#endif

static char lex[256];
static const char symbol_chars[] =
"$._ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

#define LEX_IS_SYMBOL_COMPONENT		1
#define LEX_IS_WHITESPACE		2
#define LEX_IS_LINE_SEPARATOR		3
#define LEX_IS_COMMENT_START		4
#define LEX_IS_LINE_COMMENT_START	5
#define	LEX_IS_TWOCHAR_COMMENT_1ST	6
#define	LEX_IS_TWOCHAR_COMMENT_2ND	7
#define	LEX_IS_STRINGQUOTE		8
#define	LEX_IS_COLON			9
#define	LEX_IS_NEWLINE			10
#define	LEX_IS_ONECHAR_QUOTE		11
#define IS_SYMBOL_COMPONENT(c)		(lex[c] == LEX_IS_SYMBOL_COMPONENT)
#define IS_WHITESPACE(c)		(lex[c] == LEX_IS_WHITESPACE)
#define IS_LINE_SEPARATOR(c)		(lex[c] == LEX_IS_LINE_SEPARATOR)
#define IS_COMMENT(c)			(lex[c] == LEX_IS_COMMENT_START)
#define IS_LINE_COMMENT(c)		(lex[c] == LEX_IS_LINE_COMMENT_START)
#define	IS_NEWLINE(c)			(lex[c] == LEX_IS_NEWLINE)

static int process_escape PARAMS ((int));

/* FIXME-soon: The entire lexer/parser thingy should be
   built statically at compile time rather than dynamically
   each and every time the assembler is run.  xoxorich. */

void 
do_scrub_begin ()
{
  const char *p;

  lex[' '] = LEX_IS_WHITESPACE;
  lex['\t'] = LEX_IS_WHITESPACE;
  lex['\n'] = LEX_IS_NEWLINE;
  lex[';'] = LEX_IS_LINE_SEPARATOR;
  lex[':'] = LEX_IS_COLON;

  if (! flag_mri)
    {
      lex['"'] = LEX_IS_STRINGQUOTE;

#ifndef TC_HPPA
      lex['\''] = LEX_IS_ONECHAR_QUOTE;
#endif

#ifdef SINGLE_QUOTE_STRINGS
      lex['\''] = LEX_IS_STRINGQUOTE;
#endif
    }

  /* Note: if any other character can be LEX_IS_STRINGQUOTE, the loop
     in state 5 of do_scrub_chars must be changed.  */

  /* Note that these override the previous defaults, e.g. if ';' is a
     comment char, then it isn't a line separator.  */
  for (p = symbol_chars; *p; ++p)
    {
      lex[(unsigned char) *p] = LEX_IS_SYMBOL_COMPONENT;
    }				/* declare symbol characters */

  for (p = comment_chars; *p; p++)
    {
      lex[(unsigned char) *p] = LEX_IS_COMMENT_START;
    }				/* declare comment chars */

  for (p = line_comment_chars; *p; p++)
    {
      lex[(unsigned char) *p] = LEX_IS_LINE_COMMENT_START;
    }				/* declare line comment chars */

  for (p = line_separator_chars; *p; p++)
    {
      lex[(unsigned char) *p] = LEX_IS_LINE_SEPARATOR;
    }				/* declare line separators */

  /* Only allow slash-star comments if slash is not in use */
  if (lex['/'] == 0)
    {
      lex['/'] = LEX_IS_TWOCHAR_COMMENT_1ST;
    }
  /* FIXME-soon.  This is a bad hack but otherwise, we can't do
     c-style comments when '/' is a line comment char. xoxorich. */
  if (lex['*'] == 0)
    {
      lex['*'] = LEX_IS_TWOCHAR_COMMENT_2ND;
    }

  if (flag_mri)
    {
      lex['\''] = LEX_IS_STRINGQUOTE;
      lex[';'] = LEX_IS_COMMENT_START;
      lex['*'] = LEX_IS_LINE_COMMENT_START;
      /* The MRI documentation says '!' is LEX_IS_COMMENT_START, but
         then it can't be used in an expression.  */
      lex['!'] = LEX_IS_LINE_COMMENT_START;
    }
}				/* do_scrub_begin() */

/* Saved state of the scrubber */
static int state;
static int old_state;
static char *out_string;
static char out_buf[20];
static int add_newlines;
static char *saved_input;
static int saved_input_len;

/* Data structure for saving the state of app across #include's.  Note that
   app is called asynchronously to the parsing of the .include's, so our
   state at the time .include is interpreted is completely unrelated.
   That's why we have to save it all.  */

struct app_save
  {
    int state;
    int old_state;
    char *out_string;
    char out_buf[sizeof (out_buf)];
    int add_newlines;
    char *saved_input;
    int saved_input_len;
  };

char *
app_push ()
{
  register struct app_save *saved;

  saved = (struct app_save *) xmalloc (sizeof (*saved));
  saved->state = state;
  saved->old_state = old_state;
  saved->out_string = out_string;
  memcpy (saved->out_buf, out_buf, sizeof (out_buf));
  saved->add_newlines = add_newlines;
  saved->saved_input = saved_input;
  saved->saved_input_len = saved_input_len;

  /* do_scrub_begin() is not useful, just wastes time. */

  state = 0;
  saved_input = NULL;

  return (char *) saved;
}

void 
app_pop (arg)
     char *arg;
{
  register struct app_save *saved = (struct app_save *) arg;

  /* There is no do_scrub_end (). */
  state = saved->state;
  old_state = saved->old_state;
  out_string = saved->out_string;
  memcpy (out_buf, saved->out_buf, sizeof (out_buf));
  add_newlines = saved->add_newlines;
  saved_input = saved->saved_input;
  saved_input_len = saved->saved_input_len;

  free (arg);
}				/* app_pop() */

/* @@ This assumes that \n &c are the same on host and target.  This is not
   necessarily true.  */
static int 
process_escape (ch)
     int ch;
{
  switch (ch)
    {
    case 'b':
      return '\b';
    case 'f':
      return '\f';
    case 'n':
      return '\n';
    case 'r':
      return '\r';
    case 't':
      return '\t';
    case '\'':
      return '\'';
    case '"':
      return '\"';
    default:
      return ch;
    }
}

/* This function is called to process input characters.  The GET
   parameter is used to retrieve more input characters.  GET should
   set its parameter to point to a buffer, and return the length of
   the buffer; it should return 0 at end of file.  The scrubbed output
   characters are put into the buffer starting at TOSTART; the TOSTART
   buffer is TOLEN bytes in length.  The function returns the number
   of scrubbed characters put into TOSTART.  This will be TOLEN unless
   end of file was seen.  This function is arranged as a state
   machine, and saves its state so that it may return at any point.
   This is the way the old code used to work.  */

int
do_scrub_chars (get, tostart, tolen)
     int (*get) PARAMS ((char **));
     char *tostart;
     int tolen;
{
  char *to = tostart;
  char *toend = tostart + tolen;
  char *from;
  char *fromend;
  int fromlen;
  register int ch, ch2 = 0;
  int not_cpp_line = 0;

  /*State 0: beginning of normal line
	  1: After first whitespace on line (flush more white)
	  2: After first non-white (opcode) on line (keep 1white)
	  3: after second white on line (into operands) (flush white)
	  4: after putting out a .line, put out digits
	  5: parsing a string, then go to old-state
	  6: putting out \ escape in a "d string.
	  7: After putting out a .appfile, put out string.
	  8: After putting out a .appfile string, flush until newline.
	  9: After seeing symbol char in state 3 (keep 1white after symchar)
	 10: After seeing whitespace in state 9 (keep white before symchar)
	 11: After seeing a symbol character in state 0 (eg a label definition)
	 -1: output string in out_string and go to the state in old_state
	 -2: flush text until a '*' '/' is seen, then go to state old_state
	  */

  /* I added states 9 and 10 because the MIPS ECOFF assembler uses
     constructs like ``.loc 1 20''.  This was turning into ``.loc
     120''.  States 9 and 10 ensure that a space is never dropped in
     between characters which could appear in a identifier.  Ian
     Taylor, ian@cygnus.com.

     I added state 11 so that something like "Lfoo add %r25,%r26,%r27" works
     correctly on the PA (and any other target where colons are optional).
     Jeff Law, law@cs.utah.edu.  */

  /* This macro gets the next input character.  */

#define GET()				\
  (from < fromend			\
   ? *from++				\
   : ((saved_input != NULL		\
       ? (free (saved_input),		\
	  saved_input = NULL,		\
	  0)				\
       : 0),				\
      fromlen = (*get) (&from),		\
      fromend = from + fromlen,		\
      (fromlen == 0			\
       ? EOF				\
       : *from++)))

  /* This macro pushes a character back on the input stream.  */

#define UNGET(uch) (*--from = (uch))

  /* This macro puts a character into the output buffer.  If this
     character fills the output buffer, this macro jumps to the label
     TOFULL.  We use this rather ugly approach because we need to
     handle two different termination conditions: EOF on the input
     stream, and a full output buffer.  It would be simpler if we
     always read in the entire input stream before processing it, but
     I don't want to make such a significant change to the assembler's
     memory usage.  */

#define PUT(pch)			\
  do					\
    {					\
      *to++ = (pch);			\
      if (to >= toend)			\
        goto tofull;			\
    }					\
  while (0)

  if (saved_input != NULL)
    {
      from = saved_input;
      fromend = from + saved_input_len;
    }
  else
    {
      fromlen = (*get) (&from);
      if (fromlen == 0)
	return 0;
      fromend = from + fromlen;
    }

  while (1)
    {
      /* The cases in this switch end with continue, in order to
         branch back to the top of this while loop and generate the
         next output character in the appropriate state.  */
      switch (state)
	{
	case -1:
	  ch = *out_string++;
	  if (*out_string == '\0')
	    {
	      state = old_state;
	      old_state = 3;
	    }
	  PUT (ch);
	  continue;

	case -2:
	  for (;;)
	    {
	      do
		{
		  ch = GET ();

		  if (ch == EOF)
		    {
		      as_warn ("end of file in comment");
		      goto fromeof;
		    }

		  if (ch == '\n')
		    PUT ('\n');
		}
	      while (ch != '*');

	      while ((ch = GET ()) == '*')
		;

	      if (ch == EOF)
		{
		  as_warn ("end of file in comment");
		  goto fromeof;
		}

	      if (ch == '/')
		break;

	      UNGET (ch);
	    }

	  state = old_state;
	  PUT (' ');
	  continue;

	case 4:
	  ch = GET ();
	  if (ch == EOF)
	    goto fromeof;
	  else if (ch >= '0' && ch <= '9')
	    PUT (ch);
	  else
	    {
	      while (ch != EOF && IS_WHITESPACE (ch))
		ch = GET ();
	      if (ch == '"')
		{
		  UNGET (ch);
		  out_string = "\n\t.appfile ";
		  old_state = 7;
		  state = -1;
		  PUT (*out_string++);
		}
	      else
		{
		  while (ch != EOF && ch != '\n')
		    ch = GET ();
		  state = 0;
		  PUT (ch);
		}
	    }
	  continue;

	case 5:
	  /* We are going to copy everything up to a quote character,
             with special handling for a backslash.  We try to
             optimize the copying in the simple case without using the
             GET and PUT macros.  */
	  {
	    char *s;
	    int len;

	    for (s = from; s < fromend; s++)
	      {
		ch = *s;
		/* This condition must be changed if the type of any
                   other character can be LEX_IS_STRINGQUOTE.  */
		if (ch == '\\'
		    || ch == '"'
		    || ch == '\''
		    || ch == '\n')
		  break;
	      }
	    len = s - from;
	    if (len > toend - to)
	      len = toend - to;
	    if (len > 0)
	      {
		memcpy (to, from, len);
		to += len;
		from += len;
	      }
	  }

	  ch = GET ();
	  if (ch == EOF)
	    {
	      as_warn ("end of file in string: inserted '\"'");
	      state = old_state;
	      UNGET ('\n');
	      PUT ('"');
	    }
	  else if (lex[ch] == LEX_IS_STRINGQUOTE)
	    {
	      state = old_state;
	      PUT (ch);
	    }
#ifndef NO_STRING_ESCAPES
	  else if (ch == '\\')
	    {
	      state = 6;
	      PUT (ch);
	    }
#endif
	  else if (flag_mri && ch == '\n')
	    {
	      /* Just quietly terminate the string.  This permits lines like
		   bne	label	loop if we haven't reach end yet
		 */
	      state = old_state;
	      UNGET (ch);
	      PUT ('\'');
	    }
	  else
	    {
	      PUT (ch);
	    }
	  continue;

	case 6:
	  state = 5;
	  ch = GET ();
	  switch (ch)
	    {
	      /* Handle strings broken across lines, by turning '\n' into
		 '\\' and 'n'.  */
	    case '\n':
	      UNGET ('n');
	      add_newlines++;
	      PUT ('\\');
	      continue;

	    case '"':
	    case '\\':
	    case 'b':
	    case 'f':
	    case 'n':
	    case 'r':
	    case 't':
	    case 'v':
	    case 'x':
	    case 'X':
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	      break;
#if defined(IGNORE_NONSTANDARD_ESCAPES) | defined(ONLY_STANDARD_ESCAPES)
	    default:
	      as_warn ("Unknown escape '\\%c' in string: Ignored", ch);
	      break;
#else  /* ONLY_STANDARD_ESCAPES */
	    default:
	      /* Accept \x as x for any x */
	      break;
#endif /* ONLY_STANDARD_ESCAPES */

	    case EOF:
	      as_warn ("End of file in string: '\"' inserted");
	      PUT ('"');
	      continue;
	    }
	  PUT (ch);
	  continue;

	case 7:
	  ch = GET ();
	  state = 5;
	  old_state = 8;
	  if (ch == EOF)
	    goto fromeof;
	  PUT (ch);
	  continue;

	case 8:
	  do
	    ch = GET ();
	  while (ch != '\n' && ch != EOF);
	  if (ch == EOF)
	    goto fromeof;
	  state = 0;
	  PUT (ch);
	  continue;
	}

      /* OK, we are somewhere in states 0 through 4 or 9 through 11 */

      /* flushchar: */
      ch = GET ();
    recycle:
      if (ch == EOF)
	{
	  if (state != 0)
	    {
	      as_warn ("end of file not at end of a line; newline inserted");
	      state = 0;
	      PUT ('\n');
	    }
	  goto fromeof;
	}

      switch (lex[ch])
	{
	case LEX_IS_WHITESPACE:
	  do
	    {
	      ch = GET ();
	    }
	  while (ch != EOF && IS_WHITESPACE (ch));
	  if (ch == EOF)
	    goto fromeof;

	  if (state == 0)
	    {
	      /* Preserve a single whitespace character at the
		 beginning of a line.  */
	      state = 1;
	      UNGET (ch);
	      PUT (' ');
	      break;
	    }

	  if (IS_COMMENT (ch)
	      || ch == '/'
	      || IS_LINE_SEPARATOR (ch))
	    {
	      /* cpp never outputs a leading space before the #, so
		 try to avoid being confused.  */
	      not_cpp_line = 1;
	      if (flag_mri)
		{
		  /* In MRI mode, we keep these spaces.  */
		  UNGET (ch);
		  PUT (' ');
		  break;
		}
	      goto recycle;
	    }

	  /* If we're in state 2 or 11, we've seen a non-white
	     character followed by whitespace.  If the next character
	     is ':', this is whitespace after a label name which we
	     normally must ignore.  In MRI mode, though, spaces are
	     not permitted between the label and the colon.  */
	  if ((state == 2 || state == 11)
	      && lex[ch] == LEX_IS_COLON
	      && ! flag_mri)
	    {
	      state = 1;
	      PUT (ch);
	      break;
	    }

	  switch (state)
	    {
	    case 0:
	      state++;
	      goto recycle;	/* Punted leading sp */
	    case 1:
	      /* We can arrive here if we leave a leading whitespace
		 character at the beginning of a line.  */
	      goto recycle;
	    case 2:
	      state = 3;
	      if (to + 1 < toend)
		{
		  /* Optimize common case by skipping UNGET/GET.  */
		  PUT (' ');	/* Sp after opco */
		  goto recycle;
		}
	      UNGET (ch);
	      PUT (' ');
	      break;
	    case 3:
	      if (flag_mri)
		{
		  /* In MRI mode, we keep these spaces.  */
		  UNGET (ch);
		  PUT (' ');
		  break;
		}
	      goto recycle;	/* Sp in operands */
	    case 9:
	    case 10:
	      if (flag_mri)
		{
		  /* In MRI mode, we keep these spaces.  */
		  state = 3;
		  UNGET (ch);
		  PUT (' ');
		  break;
		}
	      state = 10;	/* Sp after symbol char */
	      goto recycle;
	    case 11:
	      state = 1;
	      UNGET (ch);
	      PUT (' ');	/* Sp after label definition.  */
	      break;
	    default:
	      BAD_CASE (state);
	    }
	  break;

	case LEX_IS_TWOCHAR_COMMENT_1ST:
	  ch2 = GET ();
	  if (ch2 != EOF && lex[ch2] == LEX_IS_TWOCHAR_COMMENT_2ND)
	    {
	      for (;;)
		{
		  do
		    {
		      ch2 = GET ();
		      if (ch2 != EOF && IS_NEWLINE (ch2))
			add_newlines++;
		    }
		  while (ch2 != EOF &&
			 (lex[ch2] != LEX_IS_TWOCHAR_COMMENT_2ND));

		  while (ch2 != EOF &&
			 (lex[ch2] == LEX_IS_TWOCHAR_COMMENT_2ND))
		    {
		      ch2 = GET ();
		    }

		  if (ch2 == EOF
		      || lex[ch2] == LEX_IS_TWOCHAR_COMMENT_1ST)
		    break;
		  UNGET (ch);
		}
	      if (ch2 == EOF)
		as_warn ("end of file in multiline comment");

	      ch = ' ';
	      goto recycle;
	    }
	  else
	    {
	      if (ch2 != EOF)
		UNGET (ch2);
	      if (state == 9 || state == 10)
		state = 3;
	      PUT (ch);
	    }
	  break;

	case LEX_IS_STRINGQUOTE:
	  if (state == 10)
	    {
	      /* Preserve the whitespace in foo "bar" */
	      UNGET (ch);
	      state = 3;
	      PUT (' ');

	      /* PUT didn't jump out.  We could just break, but we
                 know what will happen, so optimize a bit.  */
	      ch = GET ();
	      old_state = 3;
	    }
	  else if (state == 9)
	    old_state = 3;
	  else
	    old_state = state;
	  state = 5;
	  PUT (ch);
	  break;

#ifndef IEEE_STYLE
	case LEX_IS_ONECHAR_QUOTE:
	  if (state == 10)
	    {
	      /* Preserve the whitespace in foo 'b' */
	      UNGET (ch);
	      state = 3;
	      PUT (' ');
	      break;
	    }
	  ch = GET ();
	  if (ch == EOF)
	    {
	      as_warn ("end of file after a one-character quote; \\0 inserted");
	      ch = 0;
	    }
	  if (ch == '\\')
	    {
	      ch = GET ();
	      if (ch == EOF)
		{
		  as_warn ("end of file in escape character");
		  ch = '\\';
		}
	      else
		ch = process_escape (ch);
	    }
	  sprintf (out_buf, "%d", (int) (unsigned char) ch);

	  /* None of these 'x constants for us.  We want 'x'.  */
	  if ((ch = GET ()) != '\'')
	    {
#ifdef REQUIRE_CHAR_CLOSE_QUOTE
	      as_warn ("Missing close quote: (assumed)");
#else
	      if (ch != EOF)
		UNGET (ch);
#endif
	    }
	  if (strlen (out_buf) == 1)
	    {
	      PUT (out_buf[0]);
	      break;
	    }
	  if (state == 9)
	    old_state = 3;
	  else
	    old_state = state;
	  state = -1;
	  out_string = out_buf;
	  PUT (*out_string++);
	  break;
#endif

	case LEX_IS_COLON:
	  if (state == 9 || state == 10)
	    state = 3;
	  else if (state != 3)
	    state = 1;
	  PUT (ch);
	  break;

	case LEX_IS_NEWLINE:
	  /* Roll out a bunch of newlines from inside comments, etc.  */
	  if (add_newlines)
	    {
	      --add_newlines;
	      UNGET (ch);
	    }
	  /* fall thru into... */

	case LEX_IS_LINE_SEPARATOR:
	  state = 0;
	  PUT (ch);
	  break;

	case LEX_IS_LINE_COMMENT_START:
	  if (state == 0)	/* Only comment at start of line.  */
	    {
	      /* FIXME-someday: The two character comment stuff was
		 badly thought out.  On i386, we want '/' as line
		 comment start AND we want C style comments.  hence
		 this hack.  The whole lexical process should be
		 reworked.  xoxorich.  */
	      if (ch == '/')
		{
		  ch2 = GET ();
		  if (ch2 == '*')
		    {
		      state = -2;
		      break;
		    }
		  else
		    {
		      UNGET (ch2);
		    }
		} /* bad hack */

	      if (ch != '#')
		not_cpp_line = 1;

	      do
		{
		  ch = GET ();
		}
	      while (ch != EOF && IS_WHITESPACE (ch));
	      if (ch == EOF)
		{
		  as_warn ("end of file in comment; newline inserted");
		  PUT ('\n');
		  break;
		}
	      if (ch < '0' || ch > '9' || not_cpp_line)
		{
		  /* Non-numerics:  Eat whole comment line */
		  while (ch != EOF && !IS_NEWLINE (ch))
		    ch = GET ();
		  if (ch == EOF)
		    as_warn ("EOF in Comment: Newline inserted");
		  state = 0;
		  PUT ('\n');
		  break;
		}
	      /* Numerics begin comment.  Perhaps CPP `# 123 "filename"' */
	      UNGET (ch);
	      old_state = 4;
	      state = -1;
	      out_string = "\t.appline ";
	      PUT (*out_string++);
	      break;
	    }

	  /* We have a line comment character which is not at the
	     start of a line.  If this is also a normal comment
	     character, fall through.  Otherwise treat it as a default
	     character.  */
	  if (strchr (comment_chars, ch) == NULL
	      && (! flag_mri
		  || (ch != '!' && ch != '*')))
	    goto de_fault;
	  if (flag_mri
	      && (ch == '!' || ch == '*')
	      && state != 1
	      && state != 10)
	    goto de_fault;
	  /* Fall through.  */
	case LEX_IS_COMMENT_START:
	  do
	    {
	      ch = GET ();
	    }
	  while (ch != EOF && !IS_NEWLINE (ch));
	  if (ch == EOF)
	    as_warn ("end of file in comment; newline inserted");
	  state = 0;
	  PUT ('\n');
	  break;

	case LEX_IS_SYMBOL_COMPONENT:
	  if (state == 10)
	    {
	      /* This is a symbol character following another symbol
		 character, with whitespace in between.  We skipped
		 the whitespace earlier, so output it now.  */
	      UNGET (ch);
	      state = 3;
	      PUT (' ');
	      break;
	    }

	  if (state == 3)
	    state = 9;

	  /* This is a common case.  Quickly copy CH and all the
             following symbol component or normal characters.  */
	  if (to + 1 < toend)
	    {
	      char *s;
	      int len;

	      for (s = from; s < fromend; s++)
		{
		  int type;

		  ch2 = *s;
		  type = lex[ch2];
		  if (type != 0
		      && type != LEX_IS_SYMBOL_COMPONENT)
		    break;
		}
	      if (s > from)
		{
		  /* Handle the last character normally, for
                     simplicity.  */
		  --s;
		}
	      len = s - from;
	      if (len > (toend - to) - 1)
		len = (toend - to) - 1;
	      if (len > 0)
		{
		  PUT (ch);
		  if (len > 8)
		    {
		      memcpy (to, from, len);
		      to += len;
		      from += len;
		    }
		  else
		    {
		      switch (len)
			{
			case 8: *to++ = *from++;
			case 7: *to++ = *from++;
			case 6: *to++ = *from++;
			case 5: *to++ = *from++;
			case 4: *to++ = *from++;
			case 3: *to++ = *from++;
			case 2: *to++ = *from++;
			case 1: *to++ = *from++;
			}
		    } 
		  ch = GET ();
		}
	    }

	  /* Fall through.  */
	default:
	de_fault:
	  /* Some relatively `normal' character.  */
	  if (state == 0)
	    {
	      state = 11;	/* Now seeing label definition */
	    }
	  else if (state == 1)
	    {
	      state = 2;	/* Ditto */
	    }
	  else if (state == 9)
	    {
	      if (lex[ch] != LEX_IS_SYMBOL_COMPONENT)
		state = 3;
	    }
	  else if (state == 10)
	    {
	      state = 3;
	    }
	  PUT (ch);
	  break;
	}
    }

  /*NOTREACHED*/

 fromeof:
  /* We have reached the end of the input.  */
  return to - tostart;

 tofull:
  /* The output buffer is full.  Save any input we have not yet
     processed.  */
  if (fromend > from)
    {
      char *save;

      save = (char *) xmalloc (fromend - from);
      memcpy (save, from, fromend - from);
      if (saved_input != NULL)
	free (saved_input);
      saved_input = save;
      saved_input_len = fromend - from;
    }
  else
    {
      if (saved_input != NULL)
	{
	  free (saved_input);
	  saved_input = NULL;
	}
    }
  return to - tostart;
}

/* end of app.c */
