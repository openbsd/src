/*		FILE WRITER			HTWrite.c
**		===========
**
*/
#include "HTUtils.h"
#include "tcp.h"

#include "HTWriter.h"

#define BUFFER_SIZE 4096	/* Tradeoff */

/*#include <stdio.h> included by HTUtils.h -- FM */

#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

/*		HTML Object
**		-----------
*/

struct _HTStream {
	CONST HTStreamClass *	isa;

	int	soc;
	char	*write_pointer;
	char	buffer[BUFFER_SIZE];
#ifdef NOT_ASCII
	BOOL	make_ascii;	/* Are we writing to the net? */
#endif
};


/*	Write the buffer out to the socket
**	----------------------------------
*/

PRIVATE void flush ARGS1(HTStream *, me)
{
    char *read_pointer	= me->buffer;
    char *write_pointer = me->write_pointer;

#ifdef NOT_ASCII
    if (me->make_ascii) {
	char * p;
	for(p = me->buffer; p < me->write_pointer; p++)
	    *p = TOASCII(*p);
    }
#endif
    while (read_pointer < write_pointer) {
	int status;
	status = NETWRITE(me->soc, me->buffer,	/* Put timeout? @@@ */
			write_pointer - read_pointer);
	if (status<0) {
	    if(TRACE) fprintf(stderr,
	    "HTWrite: Error: write() on socket returns %d !!!\n", status);
	    return;
	}
	read_pointer = read_pointer + status;
    }
    me->write_pointer = me->buffer;
}


/*_________________________________________________________________________
**
**			A C T I O N	R O U T I N E S
*/

/*	Character handling
**	------------------
*/

PRIVATE void HTWriter_put_character ARGS2(HTStream *, me, char, c)
{
    if (me->write_pointer == &me->buffer[BUFFER_SIZE]) flush(me);
    *me->write_pointer++ = c;
}



/*	String handling
**	---------------
**
**	Strings must be smaller than this buffer size.
*/
PRIVATE void HTWriter_put_string ARGS2(HTStream *, me, CONST char*, s)
{
    int l = strlen(s);
    if (me->write_pointer + l > &me->buffer[BUFFER_SIZE]) flush(me);
    strcpy(me->write_pointer, s);
    me->write_pointer = me->write_pointer + l;
}


/*	Buffer write.  Buffers can (and should!) be big.
**	------------
*/
PRIVATE void HTWriter_write ARGS3(HTStream *, me, CONST char*, s, int, l)
{

    CONST char *read_pointer	= s;
    CONST char *write_pointer = s+l;

    flush(me);		/* First get rid of our buffer */

    while (read_pointer < write_pointer) {
	int status = NETWRITE(me->soc, (char *)read_pointer,
			write_pointer - read_pointer);
	if (status<0) {
	    if(TRACE) fprintf(stderr,
	    "HTWriter_write: Error on socket output stream!!!\n");
	    return;
	}
	read_pointer = read_pointer + status;
    }
}




/*	Free an HTML object
**	-------------------
**
**	Note that the SGML parsing context is freed, but the created object is not,
**	as it takes on an existence of its own unless explicitly freed.
*/
PRIVATE void HTWriter_free ARGS1(HTStream *, me)
{
    flush(me);
    NETCLOSE(me->soc);
    FREE(me);
}

PRIVATE void HTWriter_abort ARGS2(HTStream *, me, HTError, e GCC_UNUSED)
{
    HTWriter_free(me);
}


/*	Structured Object Class
**	-----------------------
*/
PRIVATE CONST HTStreamClass HTWriter = /* As opposed to print etc */
{
	"SocketWriter",
	HTWriter_free,
	HTWriter_abort,
	HTWriter_put_character, HTWriter_put_string,
	HTWriter_write
};


/*	Subclass-specific Methods
**	-------------------------
*/

PUBLIC HTStream* HTWriter_new ARGS1(int, soc)
{
    HTStream* me = (HTStream*)malloc(sizeof(*me));
    if (me == NULL) outofmem(__FILE__, "HTML_new");
    me->isa = &HTWriter;

#ifdef NOT_ASCII
    me->make_ascii = NO;
#endif
    me->soc = soc;
    me->write_pointer = me->buffer;
    return me;
}

/*	Subclass-specific Methods
**	-------------------------
*/

PUBLIC HTStream* HTASCIIWriter ARGS1(int, soc)
{
    HTStream* me = (HTStream*)malloc(sizeof(*me));
    if (me == NULL) outofmem(__FILE__, "HTML_new");
    me->isa = &HTWriter;

#ifdef NOT_ASCII
    me->make_ascii = YES;
#endif
    me->soc = soc;
    me->write_pointer = me->buffer;
    return me;
}

