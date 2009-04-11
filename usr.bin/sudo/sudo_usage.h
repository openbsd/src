#ifndef _SUDO_USAGE_H
#define _SUDO_USAGE_H

/*
 * Usage strings for sudo.  These are here because we
 * need to be able to substitute values from configure.
 */
#define SUDO_USAGE1 " -h | -K | -k | -L | -V"
#define SUDO_USAGE2 " -v [-AknS] [-a auth_type] [-p prompt]"
#define SUDO_USAGE3 " -l[l] [-AknS] [-a auth_type] [-g groupname|#gid] [-p prompt] [-U username] [-u username|#uid] [-g groupname|#gid] [command]"
#define SUDO_USAGE4 " [-AbEHknPS] [-a auth_type] [-C fd] [-c class|-] [-g groupname|#gid] [-p prompt] [-u username|#uid] [-g groupname|#gid] [VAR=value] [-i|-s] [<command>]"
#define SUDO_USAGE5 " -e [-AknS] [-a auth_type] [-C fd] [-c class|-] [-g groupname|#gid] [-p prompt] [-u username|#uid] file ..."

#endif /* _SUDO_USAGE_H */
