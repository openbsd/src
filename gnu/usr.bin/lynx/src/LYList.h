
#ifndef LYLIST_H
#define LYLIST_H

extern char * LYlist_temp_url NOPARAMS;
extern int showlist PARAMS((document *newdoc, BOOLEAN titles));
extern void printlist PARAMS((FILE *fp, BOOLEAN titles));

#define LIST_PAGE_TITLE  "Lynx List Page"

#endif /* LYLIST_H */
