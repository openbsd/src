HOST_KEY_FILE   = $(etcdir)/ssh_host_key
HOST_CONFIG_FILE = $(etcdir)/ssh_config
SERVER_CONFIG_FILE = $(etcdir)/sshd_config

CFLAGS=	-I${.OBJDIR} -I${.CURDIR}/..

USER_SHELLS = sh jsh ksh csh tcsh bash zsh ash
