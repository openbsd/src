/*		Chunk handling:	Flexible arrays
**		===============================
**
*/

#include <HTUtils.h>
#include <HTChunk.h>

#include <LYLeaks.h>

/*	Create a chunk with a certain allocation unit
**	--------------
*/
PUBLIC HTChunk * HTChunkCreate ARGS1 (int,grow)
{
    HTChunk * ch = (HTChunk *) calloc(1, sizeof(HTChunk));
    if (ch == NULL)
        outofmem(__FILE__, "creation of chunk");

    ch->data = 0;
    ch->growby = grow;
    ch->size = 0;
    ch->allocated = 0;
    return ch;
}

/*	Create a chunk with a certain allocation unit and ensured size
**	--------------
*/
PUBLIC HTChunk * HTChunkCreate2 ARGS2 (int,grow, size_t, needed)
{
    HTChunk * ch = (HTChunk *) calloc(1, sizeof(HTChunk));
    if (ch == NULL)
        outofmem(__FILE__, "HTChunkCreate2");

    ch->growby = grow;
    if (needed > 0) {
	ch->allocated = needed-1 - ((needed-1) % ch->growby)
	    + ch->growby; /* Round up */
	ch->data = (char *)calloc(1, ch->allocated);
	if (!ch->data)
	    outofmem(__FILE__, "HTChunkCreate2 data");
    }
    ch->size = 0;
    return ch;
}


/*	Clear a chunk of all data
**	--------------------------
*/
PUBLIC void HTChunkClear ARGS1 (HTChunk *,ch)
{
    FREE(ch->data);
    ch->size = 0;
    ch->allocated = 0;
}


/*	Free a chunk
**	------------
*/
PUBLIC void HTChunkFree ARGS1 (HTChunk *,ch)
{
    FREE(ch->data);
    FREE(ch);
}


/*	Append a character
**	------------------
*/
/* Warning: the code of this function is defined as macro in SGML.c. Change
  the macro or undefine it in SGML.c when changing this function. -VH */
PUBLIC void HTChunkPutc ARGS2 (HTChunk *,ch, char,c)
{
    if (ch->size >= ch->allocated) {
	ch->allocated = ch->allocated + ch->growby;
        ch->data = ch->data ? (char *)realloc(ch->data, ch->allocated)
			    : (char *)calloc(1, ch->allocated);
      if (!ch->data)
          outofmem(__FILE__, "HTChunkPutc");
    }
    ch->data[ch->size++] = c;
}


/*	Ensure a certain size
**	---------------------
*/
PUBLIC void HTChunkEnsure ARGS2 (HTChunk *,ch, int,needed)
{
    if (needed <= ch->allocated) return;
    ch->allocated = needed-1 - ((needed-1) % ch->growby)
    			     + ch->growby; /* Round up */
    ch->data = ch->data ? (char *)realloc(ch->data, ch->allocated)
			: (char *)calloc(1, ch->allocated);
    if (ch->data == NULL)
        outofmem(__FILE__, "HTChunkEnsure");
}

PUBLIC void HTChunkPutb ARGS3 (HTChunk *,ch, CONST char *,b, int,l)
{
    int needed = ch->size + l;
    if (l <= 0) return;
    if (needed > ch->allocated) {
	ch->allocated = needed-1 - ((needed-1) % ch->growby)
	    + ch->growby; /* Round up */
        ch->data = ch->data ? (char *)realloc(ch->data, ch->allocated)
			    : (char *)calloc(1, ch->allocated);
	if (ch->data == NULL)
	    outofmem(__FILE__, "HTChunkPutb");
    }
    memcpy(ch->data + ch->size, b, l);
    ch->size += l;
}

#define PUTC(code) ch->data[ch->size++] = (char)(code)
#define PUTC2(code) ch->data[ch->size++] = (char)(0x80|(0x3f &(code)))

PUBLIC void HTChunkPutUtf8Char ARGS2(
	HTChunk *,	ch,
	UCode_t,	code)
{
    int utflen;

    if (code < 128)
	utflen = 1;
    else if   (code <     0x800L) {
	utflen = 2;
    } else if (code <   0x10000L) {
	utflen = 3;
    } else if (code <  0x200000L) {
	utflen = 4;
    } else if (code < 0x4000000L) {
	utflen = 5;
    } else if (code<=0x7fffffffL) {
	utflen = 6;
    } else
	utflen = 0;

    if (ch->size + utflen > ch->allocated) {
	int growby = (ch->growby >= utflen) ? ch->growby : utflen;
	ch->allocated = ch->allocated + growby;
        ch->data = ch->data ? (char *)realloc(ch->data, ch->allocated)
			    : (char *)calloc(1, ch->allocated);
      if (!ch->data)
          outofmem(__FILE__, "HTChunkPutUtf8Char");
    }

    switch (utflen) {
    case 0:
	return;
    case 1:
	ch->data[ch->size++] = (char)code;
	return;
    case 2:
	PUTC(0xc0 | (code>>6));
	break;
    case 3:
	PUTC(0xe0 | (code>>12));
	break;
    case 4:
	PUTC(0xf0 | (code>>18));
	break;
    case 5:
	PUTC(0xf8 | (code>>24));
	break;
    case 6:
	PUTC(0xfc | (code>>30));
    }
    switch (utflen) {
    case 6:
	PUTC2(code>>24);
    case 5:
	PUTC2(code>>18);
    case 4:
	PUTC2(code>>12);
    case 3:
	PUTC2(code>>6);
    case 2:
	PUTC2(code);
    }
}

/*	Terminate a chunk
**	-----------------
*/
PUBLIC void HTChunkTerminate ARGS1 (HTChunk *,ch)
{
    HTChunkPutc(ch, (char)0);
}


/*	Append a string
**	---------------
*/
PUBLIC void HTChunkPuts ARGS2 (HTChunk *,ch, CONST char *,s)
{
    CONST char * p;
    for (p=s; *p; p++)
        HTChunkPutc(ch, *p);
}
