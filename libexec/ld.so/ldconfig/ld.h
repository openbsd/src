/* $OpenBSD: ld.h,v 1.4 2002/02/16 21:27:30 millert Exp $ */
/*
 * Header file to make code compatible with ELF version 
 * ldconfig was taken from the a.out ld.
 */
#include <link.h>

extern int	n_search_dirs;
extern char	**search_dirs;
char	*xstrdup(char *);
void	*xmalloc(size_t);
void	*xrealloc(void *, size_t);
char	*concat(const char *, const char *, const char *);

#define PAGSIZ	__LDPGSZ
