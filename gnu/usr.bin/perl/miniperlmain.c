/*
 * "The Road goes ever on and on, down from the door where it began."
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "EXTERN.h"
#include "perl.h"

#ifdef __cplusplus
}
#  define EXTERN_C extern "C"
#else
#  define EXTERN_C extern
#endif

static void xs_init _((void));
static PerlInterpreter *my_perl;

int
#ifdef CAN_PROTOTYPE
main(int argc, char **argv, char **env)
#else
main(argc, argv, env)
int argc;
char **argv;
char **env;
#endif
{
    int exitstatus;

    PERL_SYS_INIT(&argc,&argv);

    perl_init_i18nl14n(1);

    if (!do_undump) {
	my_perl = perl_alloc();
	if (!my_perl)
	    exit(1);
	perl_construct( my_perl );
    }

    exitstatus = perl_parse( my_perl, xs_init, argc, argv, (char **) NULL );
    if (exitstatus)
	exit( exitstatus );

    exitstatus = perl_run( my_perl );

    perl_destruct( my_perl );
    perl_free( my_perl );

    PERL_SYS_TERM();

    exit( exitstatus );
}

/* Register any extra external extensions */

/* Do not delete this line--writemain depends on it */

static void
xs_init()
{
  dXSUB_SYS;
}
