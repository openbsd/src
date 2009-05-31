/*		Chunk handling:	Flexible arrays
 *		===============================
 *
 */

#include <HTUtils.h>
#include <HTChunk.h>

#include <LYLeaks.h>

/*
 *	Initialize a chunk with a certain allocation unit
 */
void HTChunkInit(HTChunk *ch, int grow)
{
    ch->data = 0;
    ch->growby = grow;
    ch->size = 0;
    ch->allocated = 0;
}

/*	Create a chunk with a certain allocation unit
 *	--------------
 */
HTChunk *HTChunkCreate(int grow)
{
    HTChunk *ch = typecalloc(HTChunk);

    if (ch == NULL)
	outofmem(__FILE__, "creation of chunk");

    HTChunkInit(ch, grow);
    return ch;
}

HTChunk *HTChunkCreateMayFail(int grow, int failok)
{
    HTChunk *ch = typecalloc(HTChunk);

    if (ch == NULL) {
	if (!failok) {
	    outofmem(__FILE__, "creation of chunk");
	} else {
	    return ch;
	}
    }
    HTChunkInit(ch, grow);
    ch->failok = failok;
    return ch;
}

/*	Create a chunk with a certain allocation unit and ensured size
 *	--------------
 */
HTChunk *HTChunkCreate2(int grow, size_t needed)
{
    HTChunk *ch = typecalloc(HTChunk);

    if (ch == NULL)
	outofmem(__FILE__, "HTChunkCreate2");

    HTChunkInit(ch, grow);
    if (needed > 0) {
	ch->allocated = needed - 1 - ((needed - 1) % ch->growby)
	    + ch->growby;	/* Round up */
	CTRACE((tfp, "HTChunkCreate2: requested %d, allocate %d\n",
		(int) needed, ch->allocated));
	ch->data = typecallocn(char, ch->allocated);

	if (!ch->data)
	    outofmem(__FILE__, "HTChunkCreate2 data");
    }
    return ch;
}

/*	Clear a chunk of all data
 *	--------------------------
 */
void HTChunkClear(HTChunk *ch)
{
    FREE(ch->data);
    ch->size = 0;
    ch->allocated = 0;
}

/*     Free a chunk (and it's chain, if any)
 *     -------------------------------------
 */
void HTChunkFree(HTChunk *ch)
{
    HTChunk *next;

    do {
	next = ch->next;
	FREE(ch->data);
	FREE(ch);
	ch = next;
    } while (ch != NULL);
}

/*	Realloc the chunk
 *	-----------------
 */
BOOL HTChunkRealloc(HTChunk *ch, int growby)
{
    char *data;

    ch->allocated = ch->allocated + growby;

    data = (ch->data
	    ? (char *) realloc(ch->data, ch->allocated)
	    : typecallocn(char, ch->allocated));

    if (data) {
	ch->data = data;
    } else if (ch->failok) {
	HTChunkClear(ch);	/* allocation failed, clear all data - kw */
	return FALSE;		/* caller should check ch->allocated - kw */
    } else {
	outofmem(__FILE__, "HTChunkRealloc");
    }
    return TRUE;
}

/*	Append a character
 *	------------------
 */
void HTChunkPutc(HTChunk *ch, char c)
{
    if (ch->size >= ch->allocated) {
	if (!HTChunkRealloc(ch, ch->growby))
	    return;
    }
    ch->data[ch->size++] = c;
}

/* like above but no realloc: extend to another chunk if necessary */
HTChunk *HTChunkPutc2(HTChunk *ch, char c)
{
    if (ch->size >= ch->allocated) {
	HTChunk *chunk = HTChunkCreateMayFail(ch->growby, ch->failok);

	ch->next = chunk;
	HTChunkPutc(chunk, c);
	return chunk;
    }
    ch->data[ch->size++] = c;
    return ch;
}

/*	Ensure a certain size
 *	---------------------
 */
void HTChunkEnsure(HTChunk *ch, int needed)
{
    if (needed <= ch->allocated)
	return;
    ch->allocated = needed - 1 - ((needed - 1) % ch->growby)
	+ ch->growby;		/* Round up */
    ch->data = (ch->data
		? (char *) realloc(ch->data, ch->allocated)
		: typecallocn(char, ch->allocated));

    if (ch->data == NULL)
	outofmem(__FILE__, "HTChunkEnsure");
}

/*
 * Append a block of characters.
 */
void HTChunkPutb(HTChunk *ch, const char *b, int l)
{
    if (l <= 0)
	return;
    if (ch->size + l > ch->allocated) {
	int growby = l - (l % ch->growby) + ch->growby;		/* Round up */

	if (!HTChunkRealloc(ch, growby))
	    return;
    }
    memcpy(ch->data + ch->size, b, l);
    ch->size += l;
}

/* like above but no realloc: extend to another chunk if necessary */
HTChunk *HTChunkPutb2(HTChunk *ch, const char *b, int l)
{
    if (l <= 0)
	return ch;
    if (ch->size + l > ch->allocated) {
	HTChunk *chunk;
	int m = ch->allocated - ch->size;

	memcpy(ch->data + ch->size, b, m);
	ch->size += m;

	chunk = HTChunkCreateMayFail(ch->growby, ch->failok);
	ch->next = chunk;
	HTChunkPutb(chunk, b + m, l - m);
	return chunk;
    }
    memcpy(ch->data + ch->size, b, l);
    ch->size += l;
    return ch;
}

#define PUTC(code)  ch->data[ch->size++] = (char)(code)
#define PUTC2(code) ch->data[ch->size++] = (char)(0x80|(0x3f &(code)))

/*
 * Append a character encoded as UTF-8.
 */
void HTChunkPutUtf8Char(HTChunk *ch, UCode_t code)
{
    int utflen;

    if (TOASCII(code) < 128)
	utflen = 1;
    else if (code < 0x800L) {
	utflen = 2;
    } else if (code < 0x10000L) {
	utflen = 3;
    } else if (code < 0x200000L) {
	utflen = 4;
    } else if (code < 0x4000000L) {
	utflen = 5;
    } else if (code <= 0x7fffffffL) {
	utflen = 6;
    } else
	utflen = 0;

    if (ch->size + utflen > ch->allocated) {
	int growby = (ch->growby >= utflen) ? ch->growby : utflen;

	if (!HTChunkRealloc(ch, growby))
	    return;
    }

    switch (utflen) {
    case 0:
	return;
    case 1:
	ch->data[ch->size++] = (char) code;
	return;
    case 2:
	PUTC(0xc0 | (code >> 6));
	break;
    case 3:
	PUTC(0xe0 | (code >> 12));
	break;
    case 4:
	PUTC(0xf0 | (code >> 18));
	break;
    case 5:
	PUTC(0xf8 | (code >> 24));
	break;
    case 6:
	PUTC(0xfc | (code >> 30));
	break;
    }
    switch (utflen) {
    case 6:
	PUTC2(code >> 24);
	/* FALLTHRU */
    case 5:
	PUTC2(code >> 18);
	/* FALLTHRU */
    case 4:
	PUTC2(code >> 12);
	/* FALLTHRU */
    case 3:
	PUTC2(code >> 6);
	/* FALLTHRU */
    case 2:
	PUTC2(code);
	break;
    }
}

/*	Terminate a chunk
 *	-----------------
 */
void HTChunkTerminate(HTChunk *ch)
{
    HTChunkPutc(ch, (char) 0);
}

/*	Append a string
 *	---------------
 */
void HTChunkPuts(HTChunk *ch, const char *s)
{
    const char *p;

    if (s != NULL) {
	for (p = s; *p; p++) {
	    if (ch->size >= ch->allocated) {
		if (!HTChunkRealloc(ch, ch->growby))
		    return;
	    }
	    ch->data[ch->size++] = *p;
	}
    }
}

/* like above but no realloc: extend to another chunk if necessary */
HTChunk *HTChunkPuts2(HTChunk *ch, const char *s)
{
    const char *p;

    if (s != NULL) {
	for (p = s; *p; p++) {
	    if (ch->size >= ch->allocated) {
		HTChunk *chunk = HTChunkCreateMayFail(ch->growby, ch->failok);

		ch->next = chunk;
		HTChunkPuts(chunk, p);
		return chunk;
	    }
	    ch->data[ch->size++] = *p;
	}
    }
    return ch;
}
