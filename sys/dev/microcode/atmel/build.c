#include <sys/types.h>
#include <fcntl.h>

#include "atuwi_intersil_fw.h"
#include "atuwi_rfmd2958-smc_fw.h"
#include "atuwi_rfmd2958_fw.h"
#include "atuwi_rfmd_fw.h"

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
	output("atu-intersil-int", atuwi_fw_intersil_int,
	    sizeof atuwi_fw_intersil_int);
	output("atu-intersil-ext", atuwi_fw_intersil_ext,
	    sizeof atuwi_fw_intersil_ext);

	output("atu-rfmd2958smc-int", atuwi_fw_rfmd2958_smc_int,
	    sizeof atuwi_fw_rfmd2958_smc_int);
	output("atu-rfmd2958smc-ext", atuwi_fw_rfmd2958_smc_ext,
	    sizeof atuwi_fw_rfmd2958_smc_ext);

	output("atu-rfmd2958-int", atuwi_fw_rfmd2958_int,
	    sizeof atuwi_fw_rfmd2958_int);
	output("atu-rfmd2958-ext", atuwi_fw_rfmd2958_ext,
	    sizeof atuwi_fw_rfmd2958_ext);

	output("atu-rfmd-int", atuwi_fw_rfmd_int,
	    sizeof atuwi_fw_rfmd_int);
	output("atu-rfmd-ext", atuwi_fw_rfmd_ext,
	    sizeof atuwi_fw_rfmd_ext);

}
