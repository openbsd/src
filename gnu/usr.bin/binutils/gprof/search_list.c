#include "libiberty.h"
#include "gprof.h"
#include "search_list.h"


void
DEFUN (search_list_append, (list, paths),
       Search_List * list AND const char *paths)
{
  Search_List_Elem *new_el;
  const char *beg, *colon;
  int len;

  colon = paths - 1;
  do
    {
      beg = colon + 1;
      colon = strchr (beg, ':');
      if (colon)
	{
	  len = colon - beg;
	}
      else
	{
	  len = strlen (beg);
	}
      new_el = (Search_List_Elem *) xmalloc (sizeof (*new_el) + len);
      memcpy (new_el->path, beg, len);
      new_el->path[len] = '\0';

      /* append new path at end of list: */
      new_el->next = 0;
      if (list->tail)
	{
	  list->tail->next = new_el;
	}
      else
	{
	  list->head = new_el;
	}
      list->tail = new_el;
    }
  while (colon);
}
