static struct def_values def_data_lecture[] = {
    { "never", never },
    { "once", once },
    { "always", always },
    { NULL, 0 },
};

static struct def_values def_data_listpw[] = {
    { "never", never },
    { "any", any },
    { "all", all },
    { "always", always },
    { NULL, 0 },
};

static struct def_values def_data_verifypw[] = {
    { "never", never },
    { "all", all },
    { "any", any },
    { "always", always },
    { NULL, 0 },
};

struct sudo_defs_types sudo_defs_table[] = {
    {
	"syslog", T_LOGFAC|T_BOOL,
	"Syslog facility if syslog is being used for logging: %s",
	NULL,
    }, {
	"syslog_goodpri", T_LOGPRI,
	"Syslog priority to use when user authenticates successfully: %s",
	NULL,
    }, {
	"syslog_badpri", T_LOGPRI,
	"Syslog priority to use when user authenticates unsuccessfully: %s",
	NULL,
    }, {
	"long_otp_prompt", T_FLAG,
	"Put OTP prompt on its own line",
	NULL,
    }, {
	"ignore_dot", T_FLAG,
	"Ignore '.' in $PATH",
	NULL,
    }, {
	"mail_always", T_FLAG,
	"Always send mail when sudo is run",
	NULL,
    }, {
	"mail_badpass", T_FLAG,
	"Send mail if user authentication fails",
	NULL,
    }, {
	"mail_no_user", T_FLAG,
	"Send mail if the user is not in sudoers",
	NULL,
    }, {
	"mail_no_host", T_FLAG,
	"Send mail if the user is not in sudoers for this host",
	NULL,
    }, {
	"mail_no_perms", T_FLAG,
	"Send mail if the user is not allowed to run a command",
	NULL,
    }, {
	"tty_tickets", T_FLAG,
	"Use a separate timestamp for each user/tty combo",
	NULL,
    }, {
	"lecture", T_TUPLE|T_BOOL,
	"Lecture user the first time they run sudo",
	def_data_lecture,
    }, {
	"lecture_file", T_STR|T_PATH|T_BOOL,
	"File containing the sudo lecture: %s",
	NULL,
    }, {
	"authenticate", T_FLAG,
	"Require users to authenticate by default",
	NULL,
    }, {
	"root_sudo", T_FLAG,
	"Root may run sudo",
	NULL,
    }, {
	"log_host", T_FLAG,
	"Log the hostname in the (non-syslog) log file",
	NULL,
    }, {
	"log_year", T_FLAG,
	"Log the year in the (non-syslog) log file",
	NULL,
    }, {
	"shell_noargs", T_FLAG,
	"If sudo is invoked with no arguments, start a shell",
	NULL,
    }, {
	"set_home", T_FLAG,
	"Set $HOME to the target user when starting a shell with -s",
	NULL,
    }, {
	"always_set_home", T_FLAG,
	"Always set $HOME to the target user's home directory",
	NULL,
    }, {
	"path_info", T_FLAG,
	"Allow some information gathering to give useful error messages",
	NULL,
    }, {
	"fqdn", T_FLAG,
	"Require fully-qualified hostnames in the sudoers file",
	NULL,
    }, {
	"insults", T_FLAG,
	"Insult the user when they enter an incorrect password",
	NULL,
    }, {
	"requiretty", T_FLAG,
	"Only allow the user to run sudo if they have a tty",
	NULL,
    }, {
	"env_editor", T_FLAG,
	"Visudo will honor the EDITOR environment variable",
	NULL,
    }, {
	"rootpw", T_FLAG,
	"Prompt for root's password, not the users's",
	NULL,
    }, {
	"runaspw", T_FLAG,
	"Prompt for the runas_default user's password, not the users's",
	NULL,
    }, {
	"targetpw", T_FLAG,
	"Prompt for the target user's password, not the users's",
	NULL,
    }, {
	"use_loginclass", T_FLAG,
	"Apply defaults in the target user's login class if there is one",
	NULL,
    }, {
	"set_logname", T_FLAG,
	"Set the LOGNAME and USER environment variables",
	NULL,
    }, {
	"stay_setuid", T_FLAG,
	"Only set the effective uid to the target user, not the real uid",
	NULL,
    }, {
	"env_reset", T_FLAG,
	"Reset the environment to a default set of variables",
	NULL,
    }, {
	"preserve_groups", T_FLAG,
	"Don't initialize the group vector to that of the target user",
	NULL,
    }, {
	"loglinelen", T_UINT|T_BOOL,
	"Length at which to wrap log file lines (0 for no wrap): %d",
	NULL,
    }, {
	"timestamp_timeout", T_INT|T_BOOL,
	"Authentication timestamp timeout: %d minutes",
	NULL,
    }, {
	"passwd_timeout", T_UINT|T_BOOL,
	"Password prompt timeout: %d minutes",
	NULL,
    }, {
	"passwd_tries", T_UINT,
	"Number of tries to enter a password: %d",
	NULL,
    }, {
	"umask", T_MODE|T_BOOL,
	"Umask to use or 0777 to use user's: 0%o",
	NULL,
    }, {
	"logfile", T_STR|T_BOOL|T_PATH,
	"Path to log file: %s",
	NULL,
    }, {
	"mailerpath", T_STR|T_BOOL|T_PATH,
	"Path to mail program: %s",
	NULL,
    }, {
	"mailerflags", T_STR|T_BOOL,
	"Flags for mail program: %s",
	NULL,
    }, {
	"mailto", T_STR|T_BOOL,
	"Address to send mail to: %s",
	NULL,
    }, {
	"mailsub", T_STR,
	"Subject line for mail messages: %s",
	NULL,
    }, {
	"badpass_message", T_STR,
	"Incorrect password message: %s",
	NULL,
    }, {
	"timestampdir", T_STR|T_PATH,
	"Path to authentication timestamp dir: %s",
	NULL,
    }, {
	"timestampowner", T_STR,
	"Owner of the authentication timestamp dir: %s",
	NULL,
    }, {
	"exempt_group", T_STR|T_BOOL,
	"Users in this group are exempt from password and PATH requirements: %s",
	NULL,
    }, {
	"passprompt", T_STR,
	"Default password prompt: %s",
	NULL,
    }, {
	"runas_default", T_STR,
	"Default user to run commands as: %s",
	NULL,
	set_runaspw,
    }, {
	"editor", T_STR|T_PATH,
	"Path to the editor for use by visudo: %s",
	NULL,
    }, {
	"listpw", T_TUPLE|T_BOOL,
	"When to require a password for 'list' pseudocommand: %s",
	def_data_listpw,
    }, {
	"verifypw", T_TUPLE|T_BOOL,
	"When to require a password for 'verify' pseudocommand: %s",
	def_data_verifypw,
    }, {
	"noexec", T_FLAG,
	"Preload the dummy exec functions contained in 'noexec_file'",
	NULL,
    }, {
	"noexec_file", T_STR|T_PATH,
	"File containing dummy exec functions: %s",
	NULL,
    }, {
	"env_check", T_LIST|T_BOOL,
	"Environment variables to check for sanity:",
	NULL,
    }, {
	"env_delete", T_LIST|T_BOOL,
	"Environment variables to remove:",
	NULL,
    }, {
	"env_keep", T_LIST|T_BOOL,
	"Environment variables to preserve:",
	NULL,
    }, {
	"ignore_local_sudoers", T_FLAG,
	"If LDAP directory is up, do we ignore local sudoers file",
	NULL,
    }, {
	"setenv", T_FLAG,
	"Allow users to set arbitrary environment variables",
	NULL,
    }, {
	NULL, 0, NULL
    }
};
