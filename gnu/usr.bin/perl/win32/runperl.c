#include <stdio.h>
#include <win32io.h>

#ifndef _DLL
extern WIN32_IOSUBSYSTEM win32stdio;
#endif

extern int RunPerl(int argc, char **argv, char **env, void *iosubsystem);

int
main(int argc, char **argv, char **env)
{
#ifdef _DLL
    return (RunPerl(argc, argv, env, NULL));
#else
    return (RunPerl(argc, argv, env, &win32stdio));
#endif
}
