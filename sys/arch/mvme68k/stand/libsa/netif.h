
#include "iodesc.h"

struct netif {
	void *devdata;
};

ssize_t		netif_get __P((struct iodesc *, void *, size_t, time_t));
ssize_t		netif_put __P((struct iodesc *, void *, size_t));

int		netif_open __P((void *));
int		netif_close __P((int));

struct iodesc	*socktodesc __P((int));

void	le_end __P((struct netif *));
void	le_error __P((struct netif *, char *, volatile void *)); 
int	le_get __P((struct iodesc *, void *, int, time_t));
void	*le_init __P((struct iodesc *));
int	le_poll __P((struct iodesc *, void *, int));
int	le_put __P((struct iodesc *, void *, int));
void	le_reset __P((struct netif *, u_char *));
