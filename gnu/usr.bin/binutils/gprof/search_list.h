#ifndef search_list_h
#define search_list_h

typedef struct search_list_elem
  {
    struct search_list_elem *next;
    char path[1];
  }
Search_List_Elem;

typedef struct
  {
    struct search_list_elem *head;
    struct search_list_elem *tail;
  }
Search_List;

extern void search_list_append PARAMS ((Search_List * list, const char *paths));

#endif /* search_list_h */
