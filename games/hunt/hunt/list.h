/*	$OpenBSD: list.h,v 1.1 1999/12/12 14:53:02 d Exp $	*/

struct driver {
	struct sockaddr addr;
	u_int16_t	response;
	int		once;
};

extern struct driver *drivers;
extern int numdrivers;
extern u_int16_t Server_port;

struct  driver *next_driver __P((void));
struct  driver *next_driver_fd __P((int));
const char *	driver_name __P((struct driver *));
void	probe_drivers __P((u_int16_t, char *));
void	probe_cleanup __P((void));

