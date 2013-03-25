use ExtUtils::MakeMaker;
use ExtUtils::Constant 0.11 'WriteConstants';
WriteMakefile(
    NAME => 'GDBM_File',
    LIBS => ["-lgdbm", "-ldbm"],
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'GDBM_File.pm',
    realclean => {FILES=> 'const-c.inc const-xs.inc'},
    XS_VERSION => eval MM->parse_version('GDBM_File.pm'), #silence warnings if we are a dev release
);
WriteConstants(
    NAME => 'GDBM_File',
    DEFAULT_TYPE => 'IV',
    BREAKOUT_AT => 8,
    PROXYSUBS => {autoload => 1},
    NAMES => [qw(GDBM_CACHESIZE GDBM_CENTFREE GDBM_COALESCEBLKS
		 GDBM_FAST GDBM_FASTMODE GDBM_INSERT GDBM_NEWDB GDBM_NOLOCK
		 GDBM_OPENMASK GDBM_READER GDBM_REPLACE GDBM_SYNC GDBM_SYNCMODE
		 GDBM_WRCREAT GDBM_WRITER)],
);
