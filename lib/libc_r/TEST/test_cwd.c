#include <stdio.h>
#include <sys/param.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
    char wd[MAXPATHLEN], *getcwd(), *getwd();

    printf("getcwd => %s\n", getcwd(wd, MAXPATHLEN));
    printf("getwd => %s\n", getwd(wd));
    exit(0);
}
