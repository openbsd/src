#include <sys/queue.h>
#include <sys/socket.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <imsg.h>

#include "extern.h"

static struct msgbuf	httpq;

void
logx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}

static void
http_request(size_t id, const char *uri, const char *last_mod, int fd)
{
	struct ibuf     *b;

	if ((b = ibuf_dynamic(256, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &id, sizeof(id));
	io_str_buffer(b, uri);
	io_str_buffer(b, last_mod);
	/* pass file as fd */
	b->fd = fd;
	ibuf_close(&httpq, b);
}

static const char *
http_result(enum http_result res)
{
	switch (res) {
	case HTTP_OK:
		return "OK";
	case HTTP_NOT_MOD:
		return "not modified";
	case HTTP_FAILED:
		return "failed";
	default:
		errx(1, "unknown http result: %d", res);
	}
}

static int
http_response(int fd)
{
	size_t id;
	enum http_result res;
	char *lastmod;

	io_simple_read(fd, &id, sizeof(id));
	io_simple_read(fd, &res, sizeof(res));
	io_str_read(fd, &lastmod);

	printf("transfer %s", http_result(res));
	if (lastmod)
		printf(", last-modified: %s" , lastmod);
	printf("\n");
	return res == HTTP_FAILED;
}

int
main(int argc, char **argv)
{
	pid_t httppid;
	int error, fd[2], outfd, http;
	int fl = SOCK_STREAM | SOCK_CLOEXEC;
	char *uri, *file, *mod;
	size_t req = 0;

	if (argc != 3 && argc != 4) {
		fprintf(stderr, "usage: test-http uri file [last-modified]\n");
		return 1;
	}
	uri = argv[1];
	file = argv[2];
	mod = argv[3];

	if (socketpair(AF_UNIX, fl, 0, fd) == -1)
		err(1, "socketpair");

	if ((httppid = fork()) == -1)
		err(1, "fork");

	if (httppid == 0) {
		close(fd[1]);

		if (pledge("stdio rpath inet dns recvfd", NULL) == -1)
			err(1, "pledge");

		proc_http(NULL, fd[0]);
		errx(1, "http process returned");
	}

	close(fd[0]);
	http = fd[1];
	msgbuf_init(&httpq);
	httpq.fd = http;

	if ((outfd = open(file, O_WRONLY|O_CREAT|O_TRUNC, 0666)) == -1)
		err(1, "open %s", file);

	http_request(req++, uri, mod, outfd);
	switch (msgbuf_write(&httpq)) {
	case 0:
		errx(1, "write: connection closed");
	case -1:
		err(1, "write");
	}
	error = http_response(http);
	return error;
}
