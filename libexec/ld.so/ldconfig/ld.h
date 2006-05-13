/* $OpenBSD: ld.h,v 1.7 2006/05/13 05:59:28 deraadt Exp $ */
/*
 * Header file to make code compatible with ELF version
 * ldconfig was taken from the a.out ld.
 */
#include <link.h>

extern int	n_search_dirs;
extern char	**search_dirs;

char	*xstrdup(const char *);
void	*xmalloc(size_t);
void	*xrealloc(void *, size_t);
void	*xcalloc(size_t, size_t);
char	*concat(const char *, const char *, const char *);

void	add_search_dir(char *name);
void	std_search_path(void);
void	add_search_path(char *path);
void	remove_search_dir(char *name);
int	getdewey(int dewey[], char *cp);
int	cmpndewey(int d1[], int n1, int d2[], int n2);

#define PAGSIZ	__LDPGSZ
