/* Public domain. */

#ifndef _LINUX_SHRINKER_H
#define _LINUX_SHRINKER_H

struct shrink_control {
	u_long	nr_to_scan;
	u_long	nr_scanned;
};

struct shrinker {
	u_long	(*count_objects)(struct shrinker *, struct shrink_control *);
	u_long	(*scan_objects)(struct shrinker *, struct shrink_control *);
	long	batch;
	int	seeks;
	TAILQ_ENTRY(shrinker) next;
};

#define SHRINK_STOP	~0UL

#define DEFAULT_SEEKS	2

int register_shrinker(struct shrinker *, const char *format, ...);
void unregister_shrinker(struct shrinker *);

static inline void
synchronize_shrinkers(void)
{
}

#endif
