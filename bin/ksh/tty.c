/*	$OpenBSD: tty.c,v 1.14 2015/10/19 14:42:16 mmcc Exp $	*/

#include <string.h>

#include "sh.h"
#include "tty.h"

int		tty_fd = -1;	/* dup'd tty file descriptor */
int		tty_devtty;	/* true if tty_fd is from /dev/tty */
struct termios	tty_state;	/* saved tty state */

void
tty_close(void)
{
	if (tty_fd >= 0) {
		close(tty_fd);
		tty_fd = -1;
	}
}

/* Initialize tty_fd.  Used for saving/reseting tty modes upon
 * foreground job completion and for setting up tty process group.
 */
void
tty_init(int init_ttystate)
{
	int	do_close = 1;
	int	tfd;

	tty_close();
	tty_devtty = 1;

	tfd = open("/dev/tty", O_RDWR, 0);
	if (tfd < 0) {
		tty_devtty = 0;
		warningf(false, "No controlling tty (open /dev/tty: %s)",
		    strerror(errno));

		do_close = 0;
		if (isatty(0))
			tfd = 0;
		else if (isatty(2))
			tfd = 2;
		else {
			warningf(false, "Can't find tty file descriptor");
			return;
		}
	}
	if ((tty_fd = fcntl(tfd, F_DUPFD_CLOEXEC, FDBASE)) < 0) {
		warningf(false, "j_ttyinit: dup of tty fd failed: %s",
		    strerror(errno));
	} else if (init_ttystate)
		tcgetattr(tty_fd, &tty_state);
	if (do_close)
		close(tfd);
}
