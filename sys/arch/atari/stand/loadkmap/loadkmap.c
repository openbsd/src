/*	$NetBSD: loadkmap.c,v 1.2 1995/07/24 05:47:48 leo Exp $	*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "../../dev/iteioctl.h"
#include "../../dev/kbdmap.h"
#include <stdio.h>


void load_kmap __P((const char *, int));
void dump_kmap(); 

int
main(argc, argv)
     int argc;
     char *argv[];
{
	int	set_sysmap = 0;
	char	*mapfile;

	if (argc > 2) {
		if ((argc == 3) && !strcmp(argv[1], "-f")) {
			mapfile = argv[2];
			set_sysmap = 1;
		}
		else {
			fprintf(stderr, "%s [-f] keymap\n", argv[0]);
			exit(1);
		}
	}
	else mapfile = argv[1];

	if (argc == 1)
		dump_kmap();
	else load_kmap(mapfile, set_sysmap);

	exit (0);
}


void
load_kmap(file, set_sysmap)
const char	*file;
int		set_sysmap;
{
	int	fd;
	char	buf[sizeof (struct kbdmap)];
	int	ioc;

	ioc = set_sysmap ? ITEIOCSSKMAP : ITEIOCSKMAP;
	
	if ((fd = open (file, 0)) >= 0) {
		if (read (fd, buf, sizeof (buf)) == sizeof (buf)) {
			if (ioctl (0, ioc, buf) == 0)
				return;
			else perror("ITEIOCSKMAP");
		}
		else perror("read kmap");

		close(fd);
	}
	else perror("open kmap");
}

void
dump_kmap()
{
	char buf[sizeof (struct kbdmap)];

	if (ioctl (0, ITEIOCGKMAP, buf) == 0)
		write (1, buf, sizeof (buf));
	else perror ("ITEIOCGKMAP");
}
