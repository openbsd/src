
#ifndef LYSHOWINFO_H
#define LYSHOWINFO_H

#ifndef LYSTRUCTS_H
#include "LYStructs.h"
#endif /* LYSTRUCTS_H */

extern int showinfo PARAMS((document *doc, int size_of_file, document *newdoc,
							char *owner_address));

#define SHOWINFO_TITLE "Information about the current document"


#endif /* LYSHOWINFO_H */

