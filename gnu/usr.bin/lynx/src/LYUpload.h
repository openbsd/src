
#ifndef LYUPLOAD_H
#define LYUPLOAD_H

#ifndef LYSTRUCTS_H
#include "LYStructs.h"
#endif /* LYSTRUCTS_H */

extern int LYUpload PARAMS((char *line));
extern int LYUpload_options PARAMS((char **newfile, char *directory));

#define UPLOAD_OPTIONS_TITLE "Lynx Upload Options"

#endif /* LYUPLOAD_H */

