/*	A small List class					      HTList.c
**	==================
**
**	A list is represented as a sequence of linked nodes of type HTList.
**	The first node is a header which contains no object.
**	New nodes are inserted between the header and the rest of the list.
*/

#include <HTUtils.h>
#include <HTList.h>

#include <LYLeaks.h>

/*	Create list.
*/
PUBLIC HTList * HTList_new NOARGS
{
    HTList *newList;

    if ((newList = typeMalloc(HTList)) == NULL)
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

/*	Reverse order of elements in list.
 */
PUBLIC HTList * HTList_reverse ARGS1(
    HTList *,		start)
{
    HTList *cur, *succ;
    if (!(start && start->next && (cur = start->next->next)))
	return start;
    start->next->next = NULL;
    while (cur) {
	succ = cur->next;
	cur->next = start->next;
	start->next = cur;
	cur = succ;
    }
    return start;
}

/*	Append a list to another.
 *
 *	If successful, the second list will become empty but not freed.
 */
PUBLIC HTList * HTList_appendList ARGS2(
    HTList *,		start,
    HTList *,		tail)
{
    HTList * temp = start;

    if (!start) {
	CTRACE((tfp, "HTList: Trying to append list %p to a nonexisting list\n",
		    tail));
	return NULL;
    }
    if (!(tail && tail->next))
	return start;

    while (temp->next)
	temp = temp->next;

    temp->next = tail->next;
    tail->next = NULL;		/* tail is now an empty list */
    return start;
}


/*	Link object to START of list (so it is pointed to by the head).
 *
 *	Unlike HTList_addObject(), it does not malloc memory for HTList entry,
 *	it use already allocated memory which should not be free'd by any
 *	list operations (optimization).
 */
PUBLIC void HTList_linkObject ARGS3(
	HTList *,	me,
	void *,		newObject,
	HTList *,	newNode)
{
    if (me) {
	if (newNode->object == NULL && newNode->next == NULL) {
	    /*  It is safe: */
	    newNode->object = newObject;
	    newNode->next = me->next;
	    me->next = newNode;

	} else {
	    /*
	     *  This node is already linked to some list (probably this one),
	     *  so refuse changing node pointers to keep the list valid!!!
	     */
	    CTRACE((tfp, "*** HTList: Refuse linking already linked obj "));
	    CTRACE((tfp, "%p, node %p, list %p\n",
			newObject, newNode, me));
	}

    } else {
	CTRACE((tfp, "HTList: Trying to link object %p to a nonexisting list\n",
		    newObject));
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
	if ((newNode = typeMalloc(HTList)) == NULL)
	    outofmem(__FILE__, "HTList_addObject");
	newNode->object = newObject;
	newNode->next = me->next;
	me->next = newNode;

    } else {
	CTRACE((tfp, "HTList: Trying to add object %p to a nonexisting list\n",
		    newObject));
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
	CTRACE((tfp, "HTList: Trying to add object %p to a nonexisting list\n",
		    newObject));
	return;
    }
    if (Pos < 0) {
	Pos = 0;
	CTRACE((tfp, "HTList: Treating negative object position %d as %d.\n",
		    pos, Pos));
    }

    prevNode = temp;
    while ((temp = temp->next)) {
	if (Pos == 0) {
	    if ((newNode = typeMalloc(HTList)) == NULL)
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


/*	Unlink specified object from list.
 *	It does not free memory.
 */
PUBLIC BOOL HTList_unlinkObject ARGS2(
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
		temp->next = NULL;
		temp->object = NULL;
		return YES;  /* Success */
	    }
	}
    }
    return NO;  /* object not found or NULL list */
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

/*	Unlink object from START of list (the Last one inserted
 *	via HTList_linkObject(), and pointed to by the head).
 *	It does not free memory.
 */
PUBLIC void * HTList_unlinkLastObject ARGS1(
	HTList *,	me)
{
    HTList * lastNode;
    void * lastObject;

    if (me && me->next) {
	lastNode = me->next;
	lastObject = lastNode->object;
	me->next = lastNode->next;
	lastNode->next = NULL;
	lastNode->object = NULL;
	return lastObject;

    } else {  /* Empty list */
	return NULL;
    }
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
