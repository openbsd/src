#ifndef LYLIST_H
#define LYLIST_H

#include <LYStructs.h>

extern char * LYlist_temp_url NOPARAMS;
extern int showlist PARAMS((document *newdoc, BOOLEAN titles));
extern void printlist PARAMS((FILE *fp, BOOLEAN titles));

#endif /* LYLIST_H */
