/* $OpenBSD: ld.h,v 1.5 2002/05/24 00:25:42 ericj Exp $ */
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
char	*concat(const char *, const char *, const char *);

#define PAGSIZ	__LDPGSZ
