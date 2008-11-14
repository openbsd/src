#ifndef _SUDO_USAGE_H
#define _SUDO_USAGE_H

/*
 * Usage strings for sudo.  These are here because we
 * need to be able to substitute values from configure.
 */
#define SUDO_USAGE1 " [-n] -h | -K | -k | -L | -V | -v"
#define SUDO_USAGE2 " -l[l] [-AnS] [-g groupname|#gid] [-U username] [-u username|#uid] [-g groupname|#gid] [command]"
#define SUDO_USAGE3 " [-AbEHnPS] [-a auth_type] [-C fd] [-c class|-] [-g groupname|#gid] [-p prompt] [-u username|#uid] [-g groupname|#gid] [VAR=value] [-i|-s] [<command>]"
#define SUDO_USAGE4 " -e [-AnS] [-a auth_type] [-C fd] [-c class|-] [-g groupname|#gid] [-p prompt] [-u username|#uid] file ..."

#endif /* _SUDO_USAGE_H */
