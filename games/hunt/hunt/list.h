/*	$OpenBSD: list.h,v 1.2 2002/02/16 21:27:10 millert Exp $	*/

struct driver {
	struct sockaddr addr;
	u_int16_t	response;
	int		once;
};

extern struct driver *drivers;
extern int numdrivers;
extern u_int16_t Server_port;

struct  driver *next_driver(void);
struct  driver *next_driver_fd(int);
const char *	driver_name(struct driver *);
void	probe_drivers(u_int16_t, char *);
void	probe_cleanup(void);

