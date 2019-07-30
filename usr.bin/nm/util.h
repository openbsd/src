/*	$OpenBSD: util.h,v 1.5 2015/08/13 19:13:28 miod Exp $	*/

/*
 * Placed in the public domain by Todd C. Miller <Todd.Miller@courtesan.com>
 * on October 9, 2004.
 */

#define	MMAP(ptr, len, prot, flags, fd, off)	do {		\
	if ((ptr = mmap(NULL, len, prot, flags, fd, off)) == MAP_FAILED) { \
		usemmap = 0;						\
		if (errno != EINVAL)					\
			warn("mmap");					\
		else if ((ptr = malloc(len)) == NULL) {			\
			ptr = MAP_FAILED;				\
			warn("malloc");					\
		} else if (pread(fd, ptr, len, off) != len) {		\
			free(ptr);					\
			ptr = MAP_FAILED;				\
			warn("pread");					\
		}							\
	}								\
} while (0)

#define MUNMAP(addr, len)	do {					\
	if (usemmap)							\
		munmap(addr, len);					\
	else								\
		free(addr);						\
	addr = NULL;							\
} while (0)

extern int usemmap;
extern int dynamic_only;

struct xnlist {
	struct nlist nl;
	unsigned long n_size;
};
