#ifndef search_list_h
#define search_list_h

/* Non-Posix systems use semi-colon as directory separator in lists,
   since colon is part of drive letter spec.  */
#if defined (__MSDOS__) || defined (_WIN32)
#define PATH_SEP_CHAR ';'
#else
#define PATH_SEP_CHAR ':'
#endif

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
