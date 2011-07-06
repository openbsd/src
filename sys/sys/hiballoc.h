#ifndef _SYS_HIBALLOC_H_
#define _SYS_HIBALLOC_H_

#include <sys/types.h>
#include <sys/tree.h>

struct hiballoc_entry;

/*
 * Hibernate allocator.
 *
 * Allocator operates from an arena, that is pre-allocated by the caller.
 */
struct hiballoc_arena
{
	RB_HEAD(hiballoc_addr, hiballoc_entry)
				hib_addrs;
};

void	*hib_alloc(struct hiballoc_arena*, size_t);
void	 hib_free(struct hiballoc_arena*, void*);
int	 hiballoc_init(struct hiballoc_arena*, void*, size_t len);

#endif /* _SYS_HIBALLOC_H_ */
