/* $OpenBSD: stdfiles.c,v 1.2 2003/11/27 22:51:36 marc Exp $ */
/* $snafu: stdfiles.c,v 1.3 2003/02/03 21:22:26 marc Exp $ */
/* PUBLIC DOMAIN Oct 2002 Marco S Hyman <marc@snafu.org> */

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

/*
 * test what happens to blocking/non-blocking mode on stdout/stderr when
 * it is changed on stdin.   A comment in the pthreads code implies that
 * all three files are linked.   Check it out.
 */

int
main(int argc, char *argv[])
{
	int dup_stdout;
	int stdin_flags;
	int stdout_flags;
	int dup_flags;
	int stderr_flags;
	int new_flags;
	int new_fd;

	/* dup stdout for the fun of it. */
	dup_stdout = dup(STDOUT_FILENO);

	/* read std in/out/err flags */
	stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	assert(stdin_flags != -1);
	stdout_flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
	assert(stdout_flags != -1);
	dup_flags = fcntl(dup_stdout, F_GETFL, 0);
	assert(dup_flags != -1);
	stderr_flags = fcntl(STDERR_FILENO, F_GETFL, 0);
	assert(stderr_flags != -1);
	printf("starting flags: in = %x, out = %x, dup = %x, err = %x\n",
	       stdin_flags, stdout_flags, dup_flags, stderr_flags);

	/* set stdin to non-blocking mode and see if stdout/stderr change */
	new_flags = stdin_flags | O_NONBLOCK;
	printf("forcing stdin to O_NONBLOCK (flags %x)\n", new_flags);
	assert(fcntl(STDIN_FILENO, F_SETFL, new_flags) != -1);

	new_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	assert(new_flags != -1);
	if (new_flags != stdin_flags) {
		printf("stdin flags changed %x -> %x\n", stdin_flags,
		       new_flags);
		stdin_flags = new_flags;
	}

	new_flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
	assert(new_flags != -1);
	if (new_flags != stdout_flags) {
		printf("stdout flags changed %x -> %x\n", stdout_flags,
		       new_flags);
		stdout_flags = new_flags;
	}

	new_flags = fcntl(dup_stdout, F_GETFL, 0);
	assert(new_flags != -1);
	if (new_flags != dup_flags) {
		printf("dup_stdout flags changed %x -> %x\n", dup_flags,
		       new_flags);
		dup_flags = new_flags;
	}

	new_flags = fcntl(STDERR_FILENO, F_GETFL, 0);
	assert(new_flags != -1);
	if (new_flags != stderr_flags) {
		printf("stderr flags changed %x -> %x\n", stderr_flags,
		       new_flags);
		stderr_flags = new_flags;
	}

	/*
	 * Close stderr and open /dev/tty.   See what it's flags
	 * are.   Set the file to non blocking.
	 */
	printf("close stderr and open /dev/tty\n");
	assert(close(STDERR_FILENO) != -1);
	new_fd = open("/dev/tty", O_RDWR|O_CREAT, 0666);
	assert(new_fd == STDERR_FILENO);
	new_flags = fcntl(STDERR_FILENO, F_GETFL, 0);
	assert(new_flags != -1);
	printf("/dev/tty [STDERR_FILENO] flags are %x\n", new_flags);
	stderr_flags = new_flags | O_NONBLOCK;
	printf("forcing /dev/tty to O_NONBLOCK (flags %x)\n", stderr_flags);
	assert(fcntl(STDERR_FILENO, F_SETFL, stdin_flags) != -1);

	/* now turn off non blocking mode on stdin */
	stdin_flags &= ~O_NONBLOCK;
	printf("turning off O_NONBLOCK on stdin (flags %x)\n", stdin_flags);
	assert(fcntl(STDIN_FILENO, F_SETFL, stdin_flags) != -1);

	new_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	assert(new_flags != -1);
	assert(new_flags == stdin_flags);

	new_flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
	assert(new_flags != -1);
	if (new_flags != stdout_flags) {
		printf("stdout flags changed %x -> %x\n", stdout_flags,
		       new_flags);
		stdout_flags = new_flags;
	}

	new_flags = fcntl(dup_stdout, F_GETFL, 0);
	assert(new_flags != -1);
	if (new_flags != dup_flags) {
		printf("dup_stdout flags changed %x -> %x\n", dup_flags,
		       new_flags);
		dup_flags = new_flags;
	}

	new_flags = fcntl(STDERR_FILENO, F_GETFL, 0);
	assert(new_flags != -1);
	if (new_flags != stderr_flags) {
		printf("stderr flags changed %x -> %x\n", stderr_flags,
		       new_flags);
		stderr_flags = new_flags;
	}

	return 0;
}
