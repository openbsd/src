#include <link.h>
extern int n_search_dirs;
extern char **search_dirs;
char *xmalloc(int size);
char *concat __P((const char *, const char *, const char *));
#define PAGSIZ                 __LDPGSZ
