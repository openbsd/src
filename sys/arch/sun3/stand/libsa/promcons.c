
#include <stdarg.h>
#include <sys/types.h>
#include <machine/mon.h>

int
getchar()
{
	return ( (*romp->getChar)() );
}

int
peekchar()
{
	return ( (*romp->mayGet)() );
}

void
putchar(c)
	int c;
{
	if (c == '\n')
		(*romp->putChar)('\r');
	(*romp->putChar)(c);
}

