
/*              List object
**
**      The list object is a generic container for storing collections
**      of things in order.
*/
#ifndef HTLIST_H
#define HTLIST_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

typedef struct _HTList HTList;

struct _HTList {
	void *		object;
	HTList *	next;
};


/*	Fast macro to traverse a list.  Call it first with copy of the list
**	header.  It returns the first object and increments the passed list
**	pointer.  Call it with the same variable until it returns NULL.
*/
#define HTList_nextObject(me) \
	((me) && ((me) = (me)->next) ? (me)->object : NULL)


/*	Macro to find object pointed to by the head (returns NULL
**	if list is empty, OR if it doesn't exist - Yuk!)
*/
#define HTList_lastObject(me) \
	((me) && (me)->next ? (me)->next->object : NULL)


/*	Macro to check if a list is empty (or doesn't exist - Yuk!)
*/
#define HTList_isEmpty(me) ((me) ? ((me)->next == NULL) : YES)


/*	Create list.
*/
extern HTList * HTList_new NOPARAMS;


/*	Delete list.
*/
extern void HTList_delete PARAMS((
	HTList *	me));

/*	Reverse a list.
*/
extern HTList * HTList_reverse PARAMS((
	HTList *	start));

/*	Append two lists, making second list empty.
*/
extern HTList * HTList_appendList PARAMS((
	HTList *	start,
	HTList *	tail));


/*      Add object to START of list (so it is pointed to by the head).
*/
extern void HTList_addObject PARAMS((
	HTList *	me,
	void *		newObject));


/*      Append object to END of list (furthest from the head).
*/
extern void HTList_appendObject PARAMS((
	HTList *	me,
	void *		newObject));


/*	Insert an object into the list at a specified position.
**      If position is 0, this places the object at the head of the list
**      and is equivalent to HTList_addObject().
*/
extern void HTList_insertObjectAt PARAMS((
	HTList *	me,
	void *		newObject,
	int		pos));


/*	Remove specified object from list.
*/
extern BOOL HTList_removeObject PARAMS((
	HTList *	me,
	void *		oldObject));


/*	Remove object at a given position in the list, where 0 is the
**	object pointed to by the head (returns a pointer to the element
**	(->object) for the object, and NULL if the list is empty, or
**	if it doesn't exist - Yuk!).
*/
extern void * HTList_removeObjectAt PARAMS((
	HTList *	me,
	int		position));


/*	Remove object from START of list (the Last one inserted
**	via HTList_addObject(), and pointed to by the head).
*/
extern void * HTList_removeLastObject PARAMS((
	HTList *	me));


/*	Remove object from END of list (the First one inserted
**	via HTList_addObject(), and furthest from the head).
*/
extern void * HTList_removeFirstObject PARAMS((
	HTList *	me));


/*	Determine total number of objects in the list,
**	not counting the head.
*/
extern int HTList_count PARAMS((
	HTList *	me));


/*	Determine position of an object in the list (a value of 0
**	means it is pointed to by the head; returns -1 if not found).
*/
extern int HTList_indexOf PARAMS((
	HTList *	me,
	void *		object));


/*	Return pointer to the object at a specified position in the list,
**	where 0 is the object pointed to by the head (returns NULL if
**	the list is empty, or if it doesn't exist - Yuk!).
*/
extern void * HTList_objectAt PARAMS((
	HTList *	me,
	int		position));

/*      Link object to START of list (so it is pointed to by the head).
 *
 *      Unlike HTList_addObject(), it does not malloc memory for HTList entry,
 *	it use already allocated memory which should not be free'd by any
 *	list operations (optimization).
 */
extern void HTList_linkObject PARAMS((
	HTList *	me,
	void *		newObject,
	HTList *	newNode));

/*	Unlink object from START of list (the Last one inserted
 *	via HTList_linkObject(), and pointed to by the head).
 *	It does not free memory.
 */
extern void * HTList_unlinkLastObject PARAMS((
	HTList *	me));

/*	Unlink specified object from list.
 *	It does not free memory.
 */
extern BOOL HTList_unlinkObject PARAMS((
	HTList *	me,
	void *		oldObject));

#endif /* HTLIST_H */
