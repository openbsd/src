use Config;
use ExtUtils::MakeMaker;

my $define = "";

if($Config{i_gdbm} && $Config{i_gdbm} eq 'define') {
    $define .= " -DHAS_GDBM";
}

WriteMakefile(
    NAME	=> 'NDBM_File',
    #LIBS => ["-L/usr/local/lib -lndbm", "-ldbm -lucb"],
    DEFINE => $define,
    MAN3PODS 	=> {}, 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'NDBM_File.pm',
    INC => ($^O eq "MacOS" ? "-i ::::db:include" : "")
);
