#include <sys/param.h>
#include <sys/device.h>

void
cpu_configure(void)
{
}

void
diskconf(void)
{
}

void
device_register(struct device *dev, void *aux)
{
}

struct nam2blk nam2blk[] = {
	{ NULL,		-1 }
};

int mainbus_match(struct device *, void *, void *);
void mainbus_attach(struct device *, struct device *, void *);

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

int
mainbus_match(struct device *parent, void *cfdata, void *aux)
{
	return 1;
}

void
mainbus_attach(struct device *parent, struct device *self, void *aux)
{
}
