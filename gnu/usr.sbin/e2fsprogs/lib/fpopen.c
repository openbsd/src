/*
 * fpopen.c --- unlike the libc popen, it directly executes the
 * command instead of call out to the shell.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#define MAX_ARGV 256

extern FILE *fpopen(const char *cmd, const char *mode);

FILE *fpopen(const char *cmd, const char *mode)
{
	char	*argv[MAX_ARGV];
	int	i = 0;
	char	*buf, *prog = 0;
	char	*p;
	int	do_stdin;
	int	fds[2];
	pid_t	pid;

	if (!mode) {
		errno = EFAULT;
		return NULL;
	}
	
	switch (*mode) {
	case 'r':
		do_stdin = 0;
		break;
	case 'w':
		do_stdin = 1;
		break;
	default:
		errno = EINVAL;
		return NULL;
	}

	/*
	 * Create the argv vector....
	 */
	buf = malloc(strlen(cmd)+1);
	if (!buf)
		return NULL;
	strcpy(buf, cmd);
	p = buf;
	while (p && *p) {
		if (isspace(*p)) {
			p++;
			continue;
		}
		if (i == 0)
			prog = p;
		argv[i++] = p;
		p = strchr(p, ' ');
		if (p)
			*p++ = 0;
	}

	argv[i] = 0;

	/*
	 * Get the pipe
	 */
	if (pipe(fds) < 0)
		return NULL;
	
	/* Fork and execute the correct program. */
	if ((pid = fork()) < 0) {
		perror("fork");
		return NULL;
	} else if (pid == 0) {
		if (do_stdin) {
			close(fds[1]);
			dup2(fds[0], 0);
		} else {
			close(fds[0]);
			dup2(fds[1], 1);
		}
		(void) execvp(prog, argv);
		perror(prog);
		exit(1);
	}
	return fdopen(do_stdin ? fds[1] : fds[0], mode);
}

