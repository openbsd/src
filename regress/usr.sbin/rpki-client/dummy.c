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

int
ip_addr_check_covered(enum afi afi,
    const unsigned char *min, const unsigned char *max,
    const struct cert_ip *ips, size_t ipsz)
{
	return -1;
}

void
ip_addr_print(const struct ip_addr *addr,
    enum afi afi, char *buf, size_t bufsz)
{
}

