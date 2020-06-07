#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

void
cpu_configure(void)
{
	config_rootfound("mainbus", NULL);
}

void
diskconf(void)
{
	printf("%s\n", __func__);
}

void
device_register(struct device *dev, void *aux)
{
}

struct nam2blk nam2blk[] = {
	{ NULL,		-1 }
};
