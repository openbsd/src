/*
 * $LynxId: HTChunk.h,v 1.20 2010/09/24 08:37:39 tom Exp $
 *
 *				     HTChunk: Flexible array handling for libwww
 *					CHUNK HANDLING:
 *					FLEXIBLE ARRAYS
 *
 * This module implements a flexible array.  It is a general utility module.  A
 * chunk is a structure which may be extended.	These routines create and
 * append data to chunks, automatically reallocating them as necessary.
 *
 */
#ifndef HTCHUNK_H
#define HTCHUNK_H 1

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#include <UCMap.h>

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct _HTChunk HTChunk;

    struct _HTChunk {
	int size;		/* In bytes                     */
	int growby;		/* Allocation unit in bytes     */
	int allocated;		/* Current size of *data        */
	char *data;		/* Pointer to malloc'd area or 0 */
	int failok;		/* allowed to fail without exiting program? */
	HTChunk *next;		/* pointer to the next chunk */
    };

/*
 * Initialize a chunk's allocation data and allocation-increment.
 */
    extern void HTChunkInit(HTChunk *ch, int grow);

/*
 *
 * Create new chunk
 *
 *   ON ENTRY,
 *
 *   growby		The number of bytes to allocate at a time when the chunk
 *			is later extended.  Arbitrary but normally a trade-off
 *			of time vs memory.
 *
 *   ON EXIT,
 *
 *   returns		A chunk pointer to the new chunk,
 *
 */

    extern HTChunk *HTChunkCreate(int growby);

/*
 *  Create a chunk for which an allocation error is not a fatal application
 *  error if failok != 0, but merely resets the chunk.  When using a chunk
 *  created this way, the caller should always check whether the contents
 *  are ok each time after data have been appended.
 *  The create call may also fail and will reurn NULL in that case. - kw
 */
    extern HTChunk *HTChunkCreateMayFail(int growby, int failok);

/*
 *  Like HTChunkCreate but with initial allocation - kw
 *
 */
    extern HTChunk *HTChunkCreate2(int growby, size_t needed);

/*
 *
 * Free a chunk
 *
 *   ON ENTRY,
 *
 *   ch			A valid chunk pointer made by HTChunkCreate()
 *
 *   ON EXIT,
 *
 *   ch			is invalid and may not be used.
 *
 */

    extern void HTChunkFree(HTChunk *ch);

/*
 *
 * Clear a chunk
 *
 *   ON ENTRY,
 *
 *   ch			A valid chunk pointer made by HTChunkCreate()
 *
 *   ON EXIT,
 *
 *   *ch		The size of the chunk is zero.
 *
 */

    extern void HTChunkClear(HTChunk *ch);

/*
 *
 * Realloc a chunk
 *
 *   ON ENTRY,
 *
 *   ch			A valid chunk pointer made by HTChunkCreate()
 *
 *   growby		growby
 *
 *   ON EXIT,
 *
 *   *ch		Expanded by growby
 *
 */

    extern BOOL HTChunkRealloc(HTChunk *ch, int growby);

/*
 *
 * Ensure a chunk has a certain space in
 *
 *   ON ENTRY,
 *
 *   ch			A valid chunk pointer made by HTChunkCreate()
 *
 *   s			The size required
 *
 *   ON EXIT,
 *
 *   *ch		Has size at least s
 *
 */

    extern void HTChunkEnsure(HTChunk *ch, int s);

/*
 *
 * Append a character to a  chunk
 *
 *   ON ENTRY,
 *
 *   ch			A valid chunk pointer made by HTChunkCreate()
 *
 *   c			The character to be appended
 *
 *   ON EXIT,
 *
 *   *ch		Is one character bigger
 *
 */
    extern void HTChunkPutc(HTChunk *ch, unsigned c);

    extern void HTChunkPutb(HTChunk *ch, const char *b, int l);

    extern void HTChunkPutUtf8Char(HTChunk *ch, UCode_t code);

/*
 * Append a string to a  chunk
 *
 *   ON ENTRY,
 *
 *   ch			A valid chunk pointer made by HTChunkCreate()
 *
 *   str		Points to a zero-terminated string to be appended
 *
 *   ON EXIT,
 *
 *   *ch		Is bigger by strlen(str)
 *
 */

    extern void HTChunkPuts(HTChunk *ch, const char *str);

/*
 *
 * Append a zero character to a  chunk
 *
 */

/*
 *
 *   ON ENTRY,
 *
 *   ch			A valid chunk pointer made by HTChunkCreate()
 *
 *   ON EXIT,
 *
 *   *ch		Is one character bigger
 *
 */

    extern void HTChunkTerminate(HTChunk *ch);

/* like the above but no realloc: extend to another chunk if necessary */
/*
 *
 * Append a character (string, data) to a chunk
 *
 *   ON ENTRY,
 *
 *   ch                        A valid chunk pointer made by HTChunkCreate()
 *
 *   c                 The character to be appended
 *
 *   ON EXIT,
 *
 *   returns           original chunk or a pointer to the new chunk
 *                     (orginal chunk is referenced to the new one
 *                     by the field 'next')
 *
 */
    extern HTChunk *HTChunkPutc2(HTChunk *ch, int c);
    extern HTChunk *HTChunkPuts2(HTChunk *ch, const char *str);
    extern HTChunk *HTChunkPutb2(HTChunk *ch, const char *b, int l);

/* New pool infrastructure: UNlike the above, store data using alignment */
    extern HTChunk *HTChunkPutb0(HTChunk *ch, const char *b, int l);

#ifdef __cplusplus
}
#endif
#endif				/* HTCHUNK_H */
