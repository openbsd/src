/*	$OpenBSD: fifo.c,v 1.1 2002/03/10 23:37:47 fgsch Exp $	*/

#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#define	FIFO_PATH	"file-fifo"

void	handler(int);

void
handler(int s)
{
	write(2, ".", 1);
}

int
main(int argc, char **argv)
{
	struct sigaction act;

	unlink(FIFO_PATH);

	if (mkfifo(FIFO_PATH, 0600) < 0)
		err(1, "mkfifo");

	act.sa_handler = handler;
	act.sa_flags = SA_RESTART;
	if (sigaction(SIGALRM, &act, NULL) < 0)
		err(1, "sigaction");

	alarm(2);

	if (open(FIFO_PATH, O_RDONLY) < 0) {
		if (errno == EINTR)
			errx(1, "open not restarted");
		else
			err(1, "open");
	}

	exit(0);
}
