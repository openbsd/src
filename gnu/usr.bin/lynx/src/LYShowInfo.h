#ifndef LYSHOWINFO_H
#define LYSHOWINFO_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

extern int showinfo PARAMS((document *doc, int size_of_file, document *newdoc,
							char *owner_address));

#endif /* LYSHOWINFO_H */

