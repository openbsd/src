/*						  HTChunk: Flexible array handling for libwww
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

typedef struct {
	int	size;		/* In bytes			*/
	int	growby;		/* Allocation unit in bytes	*/
	int	allocated;	/* Current size of *data	*/
	char *	data;		/* Pointer to malloced area or 0 */
	int	failok;		/* allowed to fail without exiting program? */
} HTChunk;


/*
 * Initialize a chunk's allocation data and allocation-increment.
 */
extern void HTChunkInit PARAMS((HTChunk * ch, int grow));

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

extern HTChunk * HTChunkCreate PARAMS((int growby));

/*
 *  Create a chunk for which an allocation error is not a fatal application
 *  error if failok != 0, but merely resets the chunk.  When using a chunk
 *  created this way, the caller should always check whether the contents
 *  are ok each time after data have been appended.
 *  The create call may also fail and will reurn NULL in that case. - kw
 */
extern HTChunk * HTChunkCreateMayFail PARAMS((int growby, int failok));

/*
 *  Like HTChunkCreate but with initial allocation - kw
 *
 */
extern HTChunk * HTChunkCreate2 PARAMS((int growby, size_t needed));


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

extern void HTChunkFree PARAMS((HTChunk * ch));


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

extern void HTChunkClear PARAMS((HTChunk * ch));


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

extern BOOL HTChunkRealloc PARAMS((HTChunk * ch, int growby));


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

extern void HTChunkEnsure PARAMS((HTChunk * ch, int s));


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
extern void HTChunkPutc PARAMS((HTChunk * ch, char c));

extern void HTChunkPutb PARAMS((HTChunk * ch, CONST char *b, int l));

extern void HTChunkPutUtf8Char PARAMS((HTChunk * ch, UCode_t code));

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


extern void HTChunkPuts PARAMS((HTChunk * ch, CONST char *str));


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


extern void HTChunkTerminate PARAMS((HTChunk * ch));

#endif /* HTCHUNK_H */
