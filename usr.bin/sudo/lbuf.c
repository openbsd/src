/*
 * Copyright (c) 2007-2009 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <ctype.h>
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
# ifdef HAVE_TERMIO_H
#  include <termio.h>
# endif
#endif

#include "sudo.h"
#include "lbuf.h"

#if !defined(TIOCGSIZE) && defined(TIOCGWINSZ)
# define TIOCGSIZE	TIOCGWINSZ
# define ttysize	winsize
# define ts_cols	ws_col
#endif

int
get_ttycols()
{
    char *p;
    int cols;
#ifdef TIOCGSIZE
    struct ttysize win;

    if (ioctl(STDERR_FILENO, TIOCGSIZE, &win) == 0 && win.ts_cols != 0)
	return((int)win.ts_cols);
#endif

    /* Fall back on $COLUMNS. */
    if ((p = getenv("COLUMNS")) == NULL || (cols = atoi(p)) <= 0)
	cols = 80;
    return(cols);
}

/*
 * TODO: add support for embedded newlines in lbufs
 */

void
lbuf_init(lbuf, buf, indent, continuation)
    struct lbuf *lbuf;
    char *buf;
    int indent;
    int continuation;
{
    lbuf->continuation = continuation;
    lbuf->indent = indent;
    lbuf->len = 0;
    lbuf->size = 0;
    lbuf->buf = NULL;
}

void
lbuf_destroy(lbuf)
    struct lbuf *lbuf;
{
    efree(lbuf->buf);
    lbuf->buf = NULL;
}

/*
 * Append strings to the buffer, expanding it as needed.
 */
void
#ifdef __STDC__
lbuf_append_quoted(struct lbuf *lbuf, const char *set, ...)
#else
lbuf_append_quoted(lbuf, set, va_alist)
	struct lbuf *lbuf;
	const char *set;
	va_dcl
#endif
{
    va_list ap;
    int len = 0;
    char *cp, *s;

#ifdef __STDC__
    va_start(ap, set);
#else
    va_start(ap);
#endif
    while ((s = va_arg(ap, char *)) != NULL) {
	len += strlen(s);
	for (cp = s; (cp = strpbrk(cp, set)) != NULL; cp++)
	    len++;
    }
    va_end(ap);

    /* Expand buffer as needed. */
    if (lbuf->len + len >= lbuf->size) {
	do {
	    lbuf->size += 256;
	} while (lbuf->len + len >= lbuf->size);
	lbuf->buf = erealloc(lbuf->buf, lbuf->size);
    }

#ifdef __STDC__
    va_start(ap, set);
#else
    va_start(ap);
#endif
    /* Append each string. */
    while ((s = va_arg(ap, char *)) != NULL) {
	while ((cp = strpbrk(s, set)) != NULL) {
	    len = (int)(cp - s);
	    memcpy(lbuf->buf + lbuf->len, s, len);
	    lbuf->len += len;
	    lbuf->buf[lbuf->len++] = '\\';
	    lbuf->buf[lbuf->len++] = *cp;
	    s = cp + 1;
	}
	if (*s != '\0') {
	    len = strlen(s);
	    memcpy(lbuf->buf + lbuf->len, s, len);
	    lbuf->len += len;
	}
    }
    lbuf->buf[lbuf->len] = '\0';
    va_end(ap);
}

/*
 * Append strings to the buffer, expanding it as needed.
 */
void
#ifdef __STDC__
lbuf_append(struct lbuf *lbuf, ...)
#else
lbuf_append(lbuf, va_alist)
	struct lbuf *lbuf;
	va_dcl
#endif
{
    va_list ap;
    int len = 0;
    char *s;

#ifdef __STDC__
    va_start(ap, lbuf);
#else
    va_start(ap);
#endif
    while ((s = va_arg(ap, char *)) != NULL)
	len += strlen(s);
    va_end(ap);

    /* Expand buffer as needed. */
    if (lbuf->len + len >= lbuf->size) {
	do {
	    lbuf->size += 256;
	} while (lbuf->len + len >= lbuf->size);
	lbuf->buf = erealloc(lbuf->buf, lbuf->size);
    }

#ifdef __STDC__
    va_start(ap, lbuf);
#else
    va_start(ap);
#endif
    /* Append each string. */
    while ((s = va_arg(ap, char *)) != NULL) {
	len = strlen(s);
	memcpy(lbuf->buf + lbuf->len, s, len);
	lbuf->len += len;
    }
    lbuf->buf[lbuf->len] = '\0';
    va_end(ap);
}

/*
 * Print the buffer with word wrap based on the tty width.
 * The lbuf is reset on return.
 */
void
lbuf_print(lbuf)
    struct lbuf *lbuf;
{
    char *cp;
    int i, have, contlen;
    static int cols = -1;

    if (cols == -1)
	cols = get_ttycols();
    contlen = lbuf->continuation ? 2 : 0;

    /* For very small widths just give up... */
    if (cols <= lbuf->indent + contlen + 20) {
	puts(lbuf->buf);
	goto done;
    }

    /*
     * Print the buffer, splitting the line as needed on a word
     * boundary.
     */
    cp = lbuf->buf;
    have = cols;
    while (cp != NULL && *cp != '\0') {
	char *ep = NULL;
	int need = lbuf->len - (int)(cp - lbuf->buf);

	if (need > have) {
	    have -= contlen;		/* subtract for continuation char */
	    if ((ep = memrchr(cp, ' ', have)) == NULL)
		ep = memchr(cp + have, ' ', need - have);
	    if (ep != NULL)
		need = (int)(ep - cp);
	}
	if (cp != lbuf->buf) {
	    /* indent continued lines */
	    for (i = 0; i < lbuf->indent; i++)
		putchar(' ');
	}
	fwrite(cp, need, 1, stdout);
	cp = ep;

	/*
	 * If there is more to print, reset have, incremement cp past
	 * the whitespace, and print a line continuaton char if needed.
	 */
	if (cp != NULL) {
	    have = cols - lbuf->indent;
	    do {
		cp++;
	    } while (isspace((unsigned char)*cp));
	    if (lbuf->continuation) {
		putchar(' ');
		putchar(lbuf->continuation);
	    }
	}
	putchar('\n');
    }

done:
    lbuf->len = 0;		/* reset the buffer for re-use. */
}
