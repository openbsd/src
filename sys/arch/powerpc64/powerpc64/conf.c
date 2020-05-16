#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/vnode.h>

#include <machine/conf.h>

struct bdevsw bdevsw[] =
{
	bdev_notdef(),
};
int	nblkdev = nitems(bdevsw);

struct cdevsw cdevsw[] =
{
	cdev_notdef(),
};
int	nchrdev = nitems(cdevsw);

dev_t	swapdev;

int
iskmemdev(dev_t dev)
{
	return 0;
}

int
iszerodev(dev_t dev)
{
	return 0;
}

dev_t
getnulldev(void)
{
	return makedev(0, 0);
}

int chrtoblktbl[] = {
	NODEV,
};
int nchrtoblktbl = nitems(chrtoblktbl);
