use ExtUtils::MakeMaker;
eval 'use ExtUtils::MakeMaker::Coverage';
require 5.006;

WriteMakefile(
    NAME            => 'Sys::Syslog',
    VERSION_FROM    => 'Syslog.pm', 
    ABSTRACT_FROM   => 'Syslog.pm', 
    INSTALLDIRS     => 'perl',
    MAN3PODS        => {}, 	# Pods will be built by installman.
    XSPROTOARG      => '-noprototypes',
    PREREQ_PM       => {
        'Test::More' => 0,
        'XSLoader' => 0,
    },
    dist            => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean           => { FILES => 'Sys-Syslog-*' }, 
    realclean       => { FILES => 'const-c.inc const-xs.inc macros.all' },
);

my $_PATH_LOG;

if (-S "/dev/log" && -w "/dev/log") {
    # Most unixes have a unix domain socket /dev/log.
    $_PATH_LOG = "/dev/log";
} elsif (-c "/dev/conslog" && -w "/dev/conslog") {
    # SunOS 5.8 has a worldwritable /dev/conslog STREAMS log driver.
    # The /dev/log STREAMS log driver on this platform has permissions
    # and ownership `crw-r----- root sys'.  /dev/conslog has more liberal
    # permissions.
    $_PATH_LOG = "/dev/conslog";
} else {
    $_PATH_LOG = "";
}

if(eval {require ExtUtils::Constant; 1}) {
    my @names = (qw( LOG_ALERT LOG_AUTH LOG_AUTHPRIV LOG_CONS LOG_CRIT LOG_CRON
                     LOG_DAEMON LOG_DEBUG LOG_EMERG LOG_ERR LOG_FACMASK LOG_FTP
                     LOG_INFO LOG_KERN LOG_LFMT LOG_LOCAL0 LOG_LOCAL1 LOG_LOCAL2
                     LOG_LOCAL3 LOG_LOCAL4 LOG_LOCAL5 LOG_LOCAL6 LOG_LOCAL7 LOG_LPR
                     LOG_MAIL LOG_NDELAY LOG_NEWS LOG_NFACILITIES LOG_NOTICE
                     LOG_NOWAIT LOG_ODELAY LOG_PERROR LOG_PID LOG_PRIMASK LOG_SYSLOG
                     LOG_USER LOG_UUCP LOG_WARNING),
           { name => "_PATH_LOG", type => "PV", default => [ "PV", qq("$_PATH_LOG") ] },
    );

    ExtUtils::Constant::WriteConstants(
        ($] > 5.009002 ? (PROXYSUBS => 1) : ()),
        NAME => 'Sys::Syslog',
        NAMES => \@names,
    );

    open(MACROS, '>macros.all') or warn "can't write 'macros.all': $!\n";
    print MACROS join $/, grep {!ref} @names;
    close(MACROS);

} else {
    use File::Copy;
    use File::Spec;
    foreach my $file ('const-c.inc', 'const-xs.inc') {
        my $fallback = File::Spec->catfile('fallback', $file);
        copy ($fallback, $file) or die "Can't copy $fallback to $ $!";
    }
}
