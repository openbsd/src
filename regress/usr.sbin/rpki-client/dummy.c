/*
 * Public domain
 * dummy shim for some tests.
 */

#include "extern.h"

struct auth *
auth_find(struct auth_tree *auths, const char *aki)
{
	return NULL;
}

int
as_check_covered(uint32_t min, uint32_t max,
    const struct cert_as *as, size_t asz)
{
	return -1;
}
