/*	$OpenBSD: alloc.c,v 1.1.1.1 1996/09/07 21:40:23 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * alloc.c
 *
 * This file contains various routines dealing with allocation and
 * deallocation of memory. And some funcions for copying text.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"

/*
 * Some memory is reserved for error messages and for being able to
 * call mf_release_all(), which needs some memory for mf_trans_add().
 */
#define KEEP_ROOM 8192L

/*
 * Note: if unsinged is 16 bits we can only allocate up to 64K with alloc().
 * Use lalloc for larger blocks.
 */
	char_u *
alloc(size)
	unsigned		size;
{
	return (lalloc((long_u)size, TRUE));
}

/*
 * alloc() with check for maximum line length
 */
	char_u *
alloc_check(size)
	unsigned		size;
{
#if !defined(UNIX) && !defined(__EMX__)
	if (sizeof(int) == 2 && size > 0x7fff)
	{
		EMSG("Line is becoming too long");
		return NULL;
	}
#endif
	return (lalloc((long_u)size, TRUE));
}

	char_u *
lalloc(size, message)
	long_u			size;
	int				message;
{
	register char_u   *p;			/* pointer to new storage space */
	static int	releasing = FALSE;	/* don't do mf_release_all() recursive */
	int			try_again;

	if (size <= 0)
	{
		EMSGN("Internal error: lalloc(%ld, )", size);
		return NULL;
	}
#if defined(MSDOS) && !defined(DJGPP)
	if (size >= 0xfff0)			/* in MSDOS we can't deal with >64K blocks */
		p = NULL;
	else
#endif

	/*
	 * If out of memory, try to release some memfile blocks.
	 * If some blocks are released call malloc again.
	 */
	for (;;)
	{
		if ((p = (char_u *)malloc(size)) != NULL)
		{
			if (mch_avail_mem(TRUE) < KEEP_ROOM && !releasing)
			{ 								/* System is low... no go! */
					vim_free((char *)p);
					p = NULL;
			}
		}
	/*
	 * Remember that mf_release_all() is being called to avoid an endless loop,
	 * because mf_release_all() may call alloc() recursively.
	 */
		if (p != NULL || releasing)
			break;
		releasing = TRUE;
		try_again = mf_release_all();
		releasing = FALSE;
		if (!try_again)
			break;
	}

	/*
	 * Avoid repeating the error message many times (they take 1 second each).
	 * Did_outofmem_msg is reset when a character is read.
	 */
	if (message && p == NULL)
		do_outofmem_msg();
	return (p);
}

	void
do_outofmem_msg()
{
	if (!did_outofmem_msg)
	{
		emsg(e_outofmem);
		did_outofmem_msg = TRUE;
	}
}

/*
 * copy a string into newly allocated memory
 */
	char_u *
strsave(string)
	char_u		   *string;
{
	char_u *p;

	p = alloc((unsigned) (STRLEN(string) + 1));
	if (p != NULL)
		STRCPY(p, string);
	return p;
}

	char_u *
strnsave(string, len)
	char_u		*string;
	int 		len;
{
	char_u *p;

	p = alloc((unsigned) (len + 1));
	if (p != NULL)
	{
		STRNCPY(p, string, len);
		p[len] = NUL;
	}
	return p;
}

/*
 * Same as strsave(), but any characters found in esc_chars are preceded by a
 * backslash.
 */
	char_u *
strsave_escaped(string, esc_chars)
	char_u		*string;
	char_u		*esc_chars;
{
	char_u		*p;
	char_u		*p2;
	char_u		*escaped_string;
	unsigned	length;

	/*
	 * First count the number of backslashes required.
	 * Then allocate the memory and insert them.
	 */
	length = 1;							/* count the trailing '/' and NUL */
	for (p = string; *p; p++)
	{
		if (vim_strchr(esc_chars, *p) != NULL)
			++length;					/* count a backslash */
		++length;						/* count an ordinary char */
	}
	escaped_string = alloc(length);
	if (escaped_string != NULL)
	{
		p2 = escaped_string;
		for (p = string; *p; p++)
		{
			if (vim_strchr(esc_chars, *p) != NULL)
				*p2++ = '\\';
			*p2++ = *p;
		}
		*p2 = NUL;
	}
	return escaped_string;
}

/*
 * copy a number of spaces
 */
	void
copy_spaces(ptr, count)
	char_u	*ptr;
	size_t	count;
{
	register size_t	i = count;
	register char_u	*p = ptr;

	while (i--)
		*p++ = ' ';
}

/*
 * delete spaces at the end of a string
 */
	void
del_trailing_spaces(ptr)
	char_u *ptr;
{
	char_u	*q;

	q = ptr + STRLEN(ptr);
	while (--q > ptr && vim_iswhite(q[0]) && q[-1] != '\\' &&
														   q[-1] != Ctrl('V'))
		*q = NUL;
}

/*
 * Isolate one part of a string option where parts are separated with commas.
 * The part is copied into buf[maxlen].
 * "*option" is advanced to the next part.
 * The length is returned.
 */
	int
copy_option_part(option, buf, maxlen, sep_chars)
	char_u		**option;
	char_u		*buf;
	int			maxlen;
	char		*sep_chars;
{
	int		len = 0;
	char_u	*p = *option;

	/* skip '.' at start of option part, for 'suffixes' */
	if (*p == '.')
		buf[len++] = *p++;
	while (*p && vim_strchr((char_u *)sep_chars, *p) == NULL)
	{
		/*
		 * Skip backslash before a separator character and space.
		 */
		if (p[0] == '\\' && vim_strchr((char_u *)sep_chars, p[1]) != NULL)
			++p;
		if (len < maxlen - 1)
			buf[len++] = *p;
		++p;
	}
	buf[len] = NUL;

	p = skip_to_option_part(p);	/* p points to next file name */

	*option = p;
	return len;
}

/*
 * replacement for free() that ignores NULL pointers
 */
	void
vim_free(x)
	void *x;
{
	if (x != NULL)
		free(x);
}

#ifndef HAVE_MEMSET
	void *
vim_memset(ptr, c, size)
	void	*ptr;
	int		c;
	size_t	size;
{
	register char *p = ptr;

	while (size-- > 0)
		*p++ = c;
	return ptr;
}
#endif

#ifdef VIM_MEMMOVE
/*
 * Version of memmove that handles overlapping source and destination.
 * For systems that don't have a function that is guaranteed to do that (SYSV).
 */
	void
vim_memmove(dst_arg, src_arg, len)
	void	*src_arg, *dst_arg;
	size_t	len;
{
	/*
	 * A void doesn't have a size, we use char pointers.
	 */
	register char *dst = dst_arg, *src = src_arg;

										/* overlap, copy backwards */
	if (dst > src && dst < src + len)
	{
		src +=len;
		dst +=len;
		while (len-- > 0)
			*--dst = *--src;
	}
	else								/* copy forwards */
		while (len-- > 0)
			*dst++ = *src++;
}
#endif

/*
 * compare two strings, ignoring case
 * return 0 for match, 1 for difference
 */
	int
vim_strnicmp(s1, s2, len)
	char_u	*s1;
	char_u	*s2;
	size_t	len;
{
	while (len)
	{
		if (TO_UPPER(*s1) != TO_UPPER(*s2))
			return 1;						/* this character different */
		if (*s1 == NUL)
			return 0;						/* strings match until NUL */
		++s1;
		++s2;
		--len;
	}
	return 0;								/* strings match */
}

/*
 * Version of strchr() and strrchr() that handle unsigned char strings
 * with characters above 128 correctly. Also it doesn't return a pointer to
 * the NUL at the end of the string.
 */
	char_u	*
vim_strchr(string, n)
	char_u	*string;
	int		n;
{
	while (*string)
	{
		if (*string == n)
			return string;
		++string;
	}
	return NULL;
}

	char_u	*
vim_strrchr(string, n)
	char_u	*string;
	int		n;
{
	char_u	*retval = NULL;

	while (*string)
	{
		if (*string == n)
			retval = string;
		++string;
	}
	return retval;
}

/*
 * Vim has its own isspace() function, because on some machines isspace()
 * can't handle characters above 128.
 */
	int
vim_isspace(x)
	int		x;
{
	return ((x >= 9 && x <= 13) || x == ' ');
}
