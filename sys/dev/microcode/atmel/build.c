#include <sys/types.h>
#include <fcntl.h>

#include "atmel_intersil_fw.h"
#include "atmel_rfmd2958-smc_fw.h"
#include "atmel_rfmd2958_fw.h"
#include "atmel_rfmd_fw.h"

void
output(const char *name, char *buf, int buflen)
{
	int i;
	int fd;

	printf("creating %s length %d\n", name, buflen);
	fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1)
		err(1, "%s", name);

	write(fd, buf, buflen);
	close(fd);
}


main(int argc, char *argv[])
{
	output("atu-intersil-int", atmel_fw_intersil_int,
	    sizeof atmel_fw_intersil_int);
	output("atu-intersil-ext", atmel_fw_intersil_ext,
	    sizeof atmel_fw_intersil_ext);

	output("atu-rfmd2958smc-int", atmel_fw_rfmd2958_smc_int,
	    sizeof atmel_fw_rfmd2958_smc_int);
	output("atu-rfmd2958smc-ext", atmel_fw_rfmd2958_smc_ext,
	    sizeof atmel_fw_rfmd2958_smc_ext);

	output("atu-rfmd2958-int", atmel_fw_rfmd2958_int,
	    sizeof atmel_fw_rfmd2958_int);
	output("atu-rfmd2958-ext", atmel_fw_rfmd2958_ext,
	    sizeof atmel_fw_rfmd2958_ext);

	output("atu-rfmd-int", atmel_fw_rfmd_int,
	    sizeof atmel_fw_rfmd_int);
	output("atu-rfmd-ext", atmel_fw_rfmd_ext,
	    sizeof atmel_fw_rfmd_ext);

}
