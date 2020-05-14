#include "config.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include "popen3.h"

static void close_pipe(int fds[2])
{
	if(fds[0] != -1) {
		close(fds[0]);
		fds[0] = -1;
	}
	if(fds[1] != -1) {
		close(fds[1]);
		fds[1] = -1;
	}
}

pid_t popen3(char *const *command,
             FILE **finptr,
             FILE **foutptr,
             FILE **ferrptr)
{
	int err = 0;
	int fdin[] = { -1, -1 };
	int fdout[] = { -1, -1 };
	int fderr[] = { -1, -1 };
	int fdsig[] = { -1, -1 };
	FILE *fin, *fout, *ferr;
	pid_t pid;
	ssize_t discard;

	if(command == NULL || *command == NULL) {
		errno = EINVAL;
		return -1;
	}

	fin = fout = ferr = NULL;

	if(finptr != NULL && (pipe(fdin) == -1 ||
			       (fin = fdopen(fdin[1], "w")) == NULL))
	{
		goto error;
	}
	if(foutptr != NULL && (pipe(fdout) == -1 ||
			        (fout = fdopen(fdout[0], "r")) == NULL))
	{
		goto error;
	}
	if(ferrptr != NULL && (pipe(fderr) == -1 ||
			        (ferr = fdopen(fderr[0], "r")) == NULL))
	{
		goto error;
	}
	if(pipe(fdsig) == -1 ||
	   fcntl(fdsig[0], F_SETFD, FD_CLOEXEC) == -1 ||
	   fcntl(fdsig[1], F_SETFD, FD_CLOEXEC) == -1)
	{
		goto error;
	}

	pid = fork();
	switch(pid) {
	case -1: /* error */
		goto error;
	case 0: /* child */
		if(ferrptr != NULL) {
			if(dup2(fderr[1], 2) == -1) {
				goto error_dup2;
			}
			close_pipe(fderr);
		} else {
			close(2);
		}
		if(foutptr != NULL) {
			if(dup2(fdout[1], 1) == -1) {
				goto error_dup2;
			}
			close_pipe(fdout);
		} else {
			close(1);
		}
		if(finptr != NULL) {
			if(dup2(fdin[0], 0) == -1) {
				goto error_dup2;
			}
			close_pipe(fdin);
		} else {
			close(0);
		}

		execvp(*command, command);
error_dup2:
		err = errno;
		close(fdsig[0]);
		discard = write(fdsig[1], &err, sizeof(err));
		(void)discard;
		close(fdsig[1]);
		exit(-1);
	default: /* parent */
	{
		/* wait for signal pipe to close */
		int ret;
		fd_set rfds;

		close(fdsig[1]);
		fdsig[1] = -1;
		do {
			FD_ZERO(&rfds);
			FD_SET(fdsig[0], &rfds);
			ret = select(fdsig[0] + 1, &rfds, NULL, NULL, NULL);
		} while(ret == -1 && errno == EINTR);

		if(ret == -1) {
			goto error;
		}

		if((ret = read(fdsig[0], &err, sizeof(err))) != 0) {
			if(ret != -1) {
				assert(ret == sizeof(err));
				errno = err;
			}
			goto error;
		}
		close(fdsig[0]);
		fdsig[0] = -1;
	}
		break;
	}

	if(finptr != NULL) {
		close(fdin[0]);
		*finptr = fin;
	}
	if(foutptr != NULL) {
		close(fdout[1]);
		*foutptr = fout;
	}
	if(ferrptr != NULL) {
		close(fderr[1]);
		*ferrptr = ferr;
	}

	return pid;

error:
	err = errno;

	if(fin != NULL) {
		fclose(fin);
		fdin[1] = -1;
	}
	if(fout != NULL) {
		fclose(fout);
		fdout[0] = -1;
	}
	if(ferr != NULL) {
		fclose(ferr);
		fderr[0] = -1;
	}

	close_pipe(fdin);
	close_pipe(fdout);
	close_pipe(fderr);
	close_pipe(fdsig);

	errno = err;

	return -1;
}
