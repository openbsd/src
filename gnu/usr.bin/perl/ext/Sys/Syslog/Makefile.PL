use ExtUtils::MakeMaker;
use ExtUtils::Constant 0.11 'WriteConstants';

WriteMakefile(
    NAME		=> 'Sys::Syslog',
    VERSION_FROM	=> 'Syslog.pm', 
    MAN3PODS		=> {}, 	# Pods will be built by installman.
    XSPROTOARG          => '-noprototypes',
    realclean => {FILES=> 'const-c.inc const-xs.inc'},
);

# We hope syslogd understands /dev/log.
#
# Solaris has a -c /dev/log, but the syslog.t #1 and #2 don't
# seem to be happy if that's _PATH_LOG.
#
my $_PATH_LOG = -S "/dev/log" ? "/dev/log" : "";

WriteConstants(
    NAME => 'Sys::Syslog',
    NAMES => [qw(LOG_ALERT LOG_AUTH LOG_AUTHPRIV LOG_CONS LOG_CRIT LOG_CRON
                 LOG_DAEMON LOG_DEBUG LOG_EMERG LOG_ERR LOG_FACMASK LOG_FTP
                 LOG_INFO LOG_KERN LOG_LFMT LOG_LOCAL0 LOG_LOCAL1 LOG_LOCAL2
                 LOG_LOCAL3 LOG_LOCAL4 LOG_LOCAL5 LOG_LOCAL6 LOG_LOCAL7 LOG_LPR
                 LOG_MAIL LOG_NDELAY LOG_NEWS LOG_NFACILITIES LOG_NOTICE
                 LOG_NOWAIT LOG_ODELAY LOG_PERROR LOG_PID LOG_PRIMASK LOG_SYSLOG
                 LOG_USER LOG_UUCP LOG_WARNING),
              {name=>"_PATH_LOG", type=>"PV", default=>["PV",qq("$_PATH_LOG")]},
             ],
);
