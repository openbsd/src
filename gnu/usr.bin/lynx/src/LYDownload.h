
#ifndef LYDOWNLOAD_H
#define LYDOWNLOAD_H

#ifndef LYSTRUCTS_H
#include "LYStructs.h"
#endif /* LYSTRUCTS_H */

extern void LYDownload PARAMS((char *line));
extern int LYdownload_options PARAMS((char **newfile, char *data_file));

#define DOWNLOAD_OPTIONS_TITLE "Lynx Download Options"

#endif /* LYDOWNLOAD_H */

