/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _def_messages.c,v 1.2 1996/08/19 08:28:12 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/localedef.h>
#include <locale.h>

const _MessagesLocale _DefaultMessagesLocale = 
{
	"^[Yn]",
	"^[Nn]",
	"yes",
	"no"
} ;

const _MessagesLocale *_CurrentMessagesLocale = &_DefaultMessagesLocale;
