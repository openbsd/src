/*	A small List class					      HTList.c
**	==================
**
**	A list is represented as a sequence of linked nodes of type HTList.
**	The first node is a header which contains no object.
**	New nodes are inserted between the header and the rest of the list.
*/

#include "HTUtils.h"
#include "HTList.h"

/*#include <stdio.h> included by HTUtils.h -- FM *//* joe@athena, TBL 921019 */

#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}


/*	Create list.
*/
PUBLIC HTList * HTList_new NOARGS
{
    HTList *newList;

    if ((newList = (HTList *)calloc(1, sizeof(HTList))) == NULL)
        outofmem(__FILE__, "HTList_new");

    newList->object = NULL;
    newList->next = NULL;

    return newList;
}


/*	Delete list.
*/
PUBLIC void HTList_delete ARGS1(
	HTList *,	me)
{
    HTList *current;

    while ((current = me)) {
      me = me->next;
      FREE (current);
    }

    return;
}


/*      Add object to START of list (so it is pointed to by the head).
*/
PUBLIC void HTList_addObject ARGS2(
	HTList *,	me,
	void *,		newObject)
{
    HTList *newNode;

    if (me) {
        if ((newNode = (HTList *)calloc(1, sizeof(HTList))) == NULL)
	    outofmem(__FILE__, "HTList_addObject");
	newNode->object = newObject;
	newNode->next = me->next;
	me->next = newNode;

    } else if (TRACE) {
        fprintf(stderr,
		"HTList: Trying to add object %p to a nonexisting list\n",
		newObject);
    }

    return;
}


/*      Append object to END of list (furthest from the head).
*/
PUBLIC void HTList_appendObject ARGS2(
	HTList *,	me,
	void *,		newObject)
{
    HTList *temp = me;

    if (temp && newObject) {
	while (temp->next)
	    temp = temp->next;
	HTList_addObject(temp, newObject);
    }

    return;
}


/*	Insert an object into the list at a specified position.
**      If position is 0, this places the object at the head of the list
**      and is equivalent to HTList_addObject().
*/
PUBLIC void HTList_insertObjectAt ARGS3(
	HTList *,	me,
	void *,		newObject,
	int,		pos)
{
    HTList * newNode;
    HTList * temp = me;
    HTList * prevNode;
    int Pos = pos;

    if (!temp) {
	if (TRACE) {
	    fprintf(stderr,
		    "HTList: Trying to add object %p to a nonexisting list\n",
		    newObject);
	}
	return;
    }
    if (Pos < 0) {
	Pos = 0;
	if (TRACE) {
	    fprintf(stderr,
		    "HTList: Treating negative object position %d as %d.\n",
		    pos, Pos);
	}
    }

    prevNode = temp;
    while ((temp = temp->next)) {
	if (Pos == 0) {
	    if ((newNode = (HTList *)calloc(1, sizeof(HTList))) == NULL)
	        outofmem(__FILE__, "HTList_addObjectAt");
	    newNode->object = newObject;
	    newNode->next = temp;
	    if (prevNode)
	        prevNode->next = newNode;
	    return;
	}
	prevNode = temp; 
	Pos--;
    }
    if (Pos >= 0)
        HTList_addObject(prevNode, newObject);

    return;
}


/*	Remove specified object from list.
*/
PUBLIC BOOL HTList_removeObject ARGS2(
	HTList *,	me,
	void *,		oldObject)
{
    HTList *temp = me;
    HTList *prevNode;

    if (temp && oldObject) {
	while (temp->next) {
	    prevNode = temp;
	    temp = temp->next;
	    if (temp->object == oldObject) {
	        prevNode->next = temp->next;
		FREE (temp);
		return YES;  /* Success */
	    }
	}
    }
    return NO;  /* object not found or NULL list */
}


/*	Remove object at a given position in the list, where 0 is the
**	object pointed to by the head (returns a pointer to the element
**	(->object) for the object, and NULL if the list is empty, or
**	if it doesn't exist - Yuk!).
*/
PUBLIC void * HTList_removeObjectAt  ARGS2(
	HTList *,	me,
	int,		position)
{
    HTList * temp = me;
    HTList * prevNode;
    int pos = position;

    if (!temp || pos < 0)
	return NULL;

    prevNode = temp;
    while ((temp = temp->next)) {
	if (pos == 0) {
	    prevNode->next = temp->next;
	    prevNode = temp;
	    FREE(temp);
	    return prevNode->object;
	}
	prevNode = temp;
	pos--;
    }

    return NULL;  /* Reached the end of the list */
}


/*	Remove object from START of list (the Last one inserted
**	via HTList_addObject(), and pointed to by the head).
*/
PUBLIC void * HTList_removeLastObject ARGS1(
	HTList *,	me)
{
    HTList * lastNode;
    void * lastObject;

    if (me && me->next) {
        lastNode = me->next;
	lastObject = lastNode->object;
	me->next = lastNode->next;
	FREE (lastNode);
	return lastObject;

    } else {  /* Empty list */
        return NULL;
    }
}


/*	Remove object from END of list (the First one inserted
**	via HTList_addObject(), and furthest from the head).
*/
PUBLIC void * HTList_removeFirstObject ARGS1(
	HTList *,	me)
{
    HTList * temp = me;
    HTList * prevNode;
    void *firstObject;

    if (!temp)
        return NULL;

    prevNode = temp;
    if (temp->next) {
	while (temp->next) {
	    prevNode = temp;
	    temp = temp->next;
	}
	firstObject = temp->object;
	prevNode->next = NULL;
	FREE (temp);
	return firstObject;

    } else {  /* Empty list */
        return NULL;
    }
}


/*	Determine total number of objects in the list,
**	not counting the head.
*/
PUBLIC int HTList_count ARGS1(
	HTList *,	me)
{
    HTList * temp = me;
    int count = 0;

    if (temp)
        while ((temp = temp->next))
	    count++;

    return count;
}


/*	Determine position of an object in the list (a value of 0
**	means it is pointed to by the head; returns -1 if not found).
*/
PUBLIC int HTList_indexOf ARGS2(
	HTList *,	me,
	void *,		object)
{
    HTList * temp = me;
    int position = 0;

    if (temp) {
	while ((temp = temp->next)) {
	    if (temp->object == object)
	        return position;
	    position++;
	}
    }

    return -1;	/* Object not in the list */
}


/*	Return pointer to the object at a specified position in the list,
**	where 0 is the object pointed to by the head (returns NULL if
**	the list is empty, or if it doesn't exist - Yuk!).
*/
PUBLIC void * HTList_objectAt ARGS2(
	HTList *,	me,
	int,		position)
{
    HTList * temp = me;
    int pos = position;

    if (!temp || pos < 0)
	return NULL;

    while ((temp = temp->next)) {
	if (pos == 0)
	    return temp->object;
	pos--;
    }

    return NULL;	/* Reached the end of the list */
}
