/*	$OpenBSD: tty.c,v 1.3 2004/12/18 20:55:52 millert Exp $	*/

#include "sh.h"
#include <sys/stat.h>
#define EXTERN
#include "tty.h"
#undef EXTERN

int
get_tty(fd, ts)
	int fd;
	TTY_state *ts;
{
	return tcgetattr(fd, ts);
}

int
set_tty(fd, ts, flags)
	int fd;
	TTY_state *ts;
	int flags;
{
	return tcsetattr(fd, TCSADRAIN, ts);
}


/* Initialize tty_fd.  Used for saving/reseting tty modes upon
 * foreground job completion and for setting up tty process group.
 */
void
tty_init(init_ttystate)
	int init_ttystate;
{
	int	do_close = 1;
	int	tfd;

	if (tty_fd >= 0) {
		close(tty_fd);
		tty_fd = -1;
	}
	tty_devtty = 1;

	if ((tfd = open("/dev/tty", O_RDWR, 0)) < 0) {

		if (tfd < 0) {
			tty_devtty = 0;
			warningf(FALSE,
				"No controlling tty (open /dev/tty: %s)",
				strerror(errno));
		}
	}

	if (tfd < 0) {
		do_close = 0;
		if (isatty(0))
			tfd = 0;
		else if (isatty(2))
			tfd = 2;
		else {
			warningf(FALSE, "Can't find tty file descriptor");
			return;
		}
	}
	if ((tty_fd = fcntl(tfd, F_DUPFD, FDBASE)) < 0) {
		warningf(FALSE, "j_ttyinit: dup of tty fd failed: %s",
			strerror(errno));
	} else if (fd_clexec(tty_fd) < 0) {
		warningf(FALSE, "j_ttyinit: can't set close-on-exec flag: %s",
			strerror(errno));
		close(tty_fd);
		tty_fd = -1;
	} else if (init_ttystate)
		get_tty(tty_fd, &tty_state);
	if (do_close)
		close(tfd);
}

void
tty_close()
{
	if (tty_fd >= 0) {
		close(tty_fd);
		tty_fd = -1;
	}
}
