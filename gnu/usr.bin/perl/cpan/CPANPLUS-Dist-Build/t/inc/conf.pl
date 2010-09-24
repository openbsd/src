### On VMS, the ENV is not reset after the program terminates.
### So reset it here explicitly
my ($old_env_path, $old_env_perl5lib);
BEGIN {
    use FindBin; 
    use File::Spec;
    
    ### paths to our own 'lib' and 'inc' dirs
    ### include them, relative from t/
    my @paths   = map { "$FindBin::Bin/$_" } qw[../lib inc];

    ### absolute'ify the paths in @INC;
    my @rel2abs = map { File::Spec->rel2abs( $_ ) }
                    grep { not File::Spec->file_name_is_absolute( $_ ) } @INC;
    
    ### use require to make devel::cover happy
    require lib;
    for ( @paths, @rel2abs ) { 
        my $l = 'lib'; 
        $l->import( $_ ) 
    }

    use Config;

    ### and add them to the environment, so shellouts get them
    $old_env_perl5lib = $ENV{'PERL5LIB'};
    $ENV{'PERL5LIB'}  = join $Config{'path_sep'}, 
                        grep { defined } $ENV{'PERL5LIB'}, @paths, @rel2abs;
    
    ### add CPANPLUS' bin dir to the front of $ENV{PATH}, so that cpanp-run-perl
    ### and friends get picked up, only under PERL_CORE though.
    $old_env_path = $ENV{PATH};
    if ( $ENV{PERL_CORE} ) {
       $ENV{'PATH'}  = join $Config{'path_sep'}, 
                    grep { defined } "$FindBin::Bin/../../../utils", $ENV{'PATH'};
    }

    ### Fix up the path to perl, as we're about to chdir
    ### but only under perlcore, or if the path contains delimiters,
    ### meaning it's relative, but not looked up in your $PATH
    $^X = File::Spec->rel2abs( $^X ) 
        if $ENV{PERL_CORE} or ( $^X =~ m|[/\\]| );

    ### chdir to our own test dir, so we know all files are relative 
    ### to this point, no matter whether run from perlcore tests or
    ### regular CPAN installs
    chdir "$FindBin::Bin" if -d "$FindBin::Bin"
}

BEGIN {
    use IPC::Cmd;
   
    ### Win32 has issues with redirecting FD's properly in IPC::Run:
    ### Can't redirect fd #4 on Win32 at IPC/Run.pm line 2801
    $IPC::Cmd::USE_IPC_RUN = 0 if $^O eq 'MSWin32';
    $IPC::Cmd::USE_IPC_RUN = 0 if $^O eq 'MSWin32';
}

### Use a $^O comparison, as depending on module at this time
### may cause weird errors/warnings
END {
    if ($^O eq 'VMS') {
        ### VMS environment variables modified by this test need to be put back
        ### path is "magic" on VMS, we can not tell if it really existed before
        ### this was run, because VMS will magically pretend that a PATH
        ### environment variable exists set to the current working directory
        $ENV{PATH} = $old_env_path;

        if (defined $old_env_perl5lib) {
            $ENV{PERL5LIB} = $old_env_perl5lib;
        } else {
            delete $ENV{PERL5LIB};
        }
    }
}

use strict;
use CPANPLUS::Configure;
use CPANPLUS::Error ();

use File::Path      qw[rmtree];
use FileHandle;
use File::Basename  qw[basename];

{   ### Force the ignoring of .po files for L::M::S
    $INC{'Locale::Maketext::Lexicon.pm'} = __FILE__;
    $Locale::Maketext::Lexicon::VERSION = 0;
}

my $Env = 'PERL5_CPANPLUS_TEST_VERBOSE';

# prereq has to be in our package file && core!
use constant TEST_CONF_PREREQ           => 'Cwd';   
use constant TEST_CONF_MODULE           => 'Foo::Bar::EU::NOXS';
use constant TEST_CONF_MODULE_SUB       => 'Foo::Bar::EU::NOXS::Sub';
use constant TEST_CONF_AUTHOR           => 'EUNOXS';
use constant TEST_CONF_INST_MODULE      => 'Foo::Bar';
use constant TEST_CONF_INVALID_MODULE   => 'fnurk';
use constant TEST_CONF_MIRROR_DIR       => 'dummy-localmirror';
use constant TEST_CONF_CPAN_DIR         => 'dummy-CPAN';
use constant TEST_CONF_CPANPLUS_DIR     => 'dummy-cpanplus';
use constant TEST_CONF_INSTALL_DIR      => File::Spec->rel2abs(
                                                File::Spec->catdir(      
                                                    TEST_CONF_CPANPLUS_DIR,
                                                    'install'
                                                )
                                            );       

### we might need this Some Day when we're installing into
### our own sandbox. see t/20.t for details
# use constant TEST_INSTALL_DIR       => do {
#     my $dir = File::Spec->rel2abs( 'dummy-perl' );
# 
#     ### clean up paths if we are on win32    
#     ### dirs with spaces will be.. bad :(
#     $^O eq 'MSWin32'
#         ? Win32::GetShortPathName( $dir )
#         : $dir;
# };        

# use constant TEST_INSTALL_DIR_LIB 
#     => File::Spec->catdir( TEST_INSTALL_DIR, 'lib' );
# use constant TEST_INSTALL_DIR_BIN 
#     => File::Spec->catdir( TEST_INSTALL_DIR, 'bin' );
# use constant TEST_INSTALL_DIR_MAN1 
#     => File::Spec->catdir( TEST_INSTALL_DIR, 'man', 'man1' );
# use constant TEST_INSTALL_DIR_MAN3
#     => File::Spec->catdir( TEST_INSTALL_DIR, 'man', 'man3' );
# use constant TEST_INSTALL_DIR_ARCH
#     => File::Spec->catdir( TEST_INSTALL_DIR, 'arch' );
# 
# use constant TEST_INSTALL_EU_MM_FLAGS =>
#     ' INSTALLDIRS=site' .
#     ' INSTALLSITELIB='     . TEST_INSTALL_DIR_LIB .
#     ' INSTALLSITEARCH='    . TEST_INSTALL_DIR_ARCH .    # .packlist
#     ' INSTALLARCHLIB='     . TEST_INSTALL_DIR_ARCH .    # perllocal.pod
#     ' INSTALLSITEBIN='     . TEST_INSTALL_DIR_BIN .
#     ' INSTALLSCRIPT='      . TEST_INSTALL_DIR_BIN .
#     ' INSTALLSITEMAN1DIR=' . TEST_INSTALL_DIR_MAN1 .
#     ' INSTALLSITEMAN3DIR=' . TEST_INSTALL_DIR_MAN3;


sub dummy_cpan_dir {
    ### VMS needs this in directory format for rel2abs
    my $test_dir = $^O eq 'VMS'
                    ? File::Spec->catdir(TEST_CONF_CPAN_DIR)
                    : TEST_CONF_CPAN_DIR;

    ### Convert to an absolute file specification
    my $abs_test_dir = File::Spec->rel2abs($test_dir);
    
    ### According to John M: the hosts path needs to be in UNIX format.  
    ### File::Spec::Unix->rel2abs does not work at all on VMS
    $abs_test_dir    = VMS::Filespec::unixify( $abs_test_dir ) if $^O eq 'VMS';

    return $abs_test_dir;
}

sub gimme_conf { 

    ### don't load any other configs than the heuristic one
    ### during tests. They might hold broken/incorrect data
    ### for our test suite. Bug [perl #43629] showed this.
    my $conf = CPANPLUS::Configure->new( load_configs => 0 );

    my $dummy_cpan = dummy_cpan_dir();
    
    $conf->set_conf( hosts  => [ { 
                        path        => $dummy_cpan,
                        scheme      => 'file',
                    } ],      
    );
    $conf->set_conf( base       => File::Spec->rel2abs(TEST_CONF_CPANPLUS_DIR));
    $conf->set_conf( dist_type  => '' );
    $conf->set_conf( signature  => 0 );
    $conf->set_conf( verbose    => 1 ) if $ENV{ $Env };
    
    ### never use a pager in the test suite
    $conf->set_program( pager   => '' );

    ### dmq tells us that we should run with /nologo
    ### if using nmake, as it's very noisy otherwise.
    {   my $make = $conf->get_program('make');
        if( $make and basename($make) =~ /^nmake/i ) {
            $conf->set_conf( makeflags => '/nologo' );
        }
    }

    ### CPANPLUS::Config checks 3 specific scenarios first
    ### when looking for cpanp-run-perl: parallel to cpanp,
    ### parallel to CPANPLUS.pm, or installed into a custom
    ### prefix like /tmp/foo. Only *THEN* does it check the
    ### the path.
    ### If the perl core is extracted to a directory that has
    ### cpanp-run-perl installed the same amount of 'uplevels'
    ### as the /tmp/foo prefix, we'll pull in the wrong script
    ### by accident.
    ### Since we set the path to cpanp-run-perl explicitily
    ### at the top of this script, it's best to update the config
    ### ourselves with a path lookup, rather than rely on its
    ### heuristics. Thanks to David Wheeler, Josh Jore and Vincent
    ### Pit for helping to track this down.
    if( $ENV{PERL_CORE} ) {
        $conf->set_program( "perlwrapper" => IPC::Cmd::can_run('cpanp-run-perl') );
    }

    $conf->set_conf( source_engine =>  $ENV{CPANPLUS_SOURCE_ENGINE} )
        if $ENV{CPANPLUS_SOURCE_ENGINE};
    
    _clean_test_dir( [
        $conf->get_conf('base'),     
        TEST_CONF_MIRROR_DIR,
#         TEST_INSTALL_DIR_LIB,
#         TEST_INSTALL_DIR_BIN,
#         TEST_INSTALL_DIR_MAN1, 
#         TEST_INSTALL_DIR_MAN3,
    ], (  $ENV{PERL_CORE} ? 0 : 1 ) );
        
    return $conf;
};

# placeholder

### clean these files if we're under perl core
END { 
    if ( $ENV{PERL_CORE} ) {

        _clean_test_dir( [
            gimme_conf->get_conf('base'),   
            TEST_CONF_MIRROR_DIR,
    #         TEST_INSTALL_DIR_LIB,
    #         TEST_INSTALL_DIR_BIN,
    #         TEST_INSTALL_DIR_MAN1, 
    #         TEST_INSTALL_DIR_MAN3,
        ], 0 ); # DO NOT be verbose under perl core -- makes tests fail
    }
}

### whenever we start a new script, we want to clean out our
### old files from the test '.cpanplus' dir..
sub _clean_test_dir {
    my $dirs    = shift || [];
    my $verbose = shift || 0;

    for my $dir ( @$dirs ) {

        ### no point if it doesn't exist;
        next unless -d $dir;

        my $dh;
        opendir $dh, $dir or die "Could not open basedir '$dir': $!";
        while( my $file = readdir $dh ) { 
            next if $file =~ /^\./;  # skip dot files
            
            my $path = File::Spec->catfile( $dir, $file );
            
            ### directory, rmtree it
            if( -d $path ) {

                ### John Malmberg reports yet another VMS issue:
                ### A directory name on VMS in VMS format ends with .dir 
                ### when it is referenced as a file.
                ### In UNIX format traditionally PERL on VMS does not remove the
                ### '.dir', however the VMS C library conversion routines do
                ### remove the '.dir' and the VMS C library routines can not 
                ### handle the '.dir' being present on UNIX format filenames.
                ### So code doing the fixup has on VMS has to be able to handle 
                ### both UNIX format names and VMS format names. 
                
                ### XXX See http://www.xray.mpe.mpg.de/
                ### mailing-lists/perl5-porters/2007-10/msg00064.html
                ### for details -- the below regex could use some touchups
                ### according to John. M.            
                $file =~ s/\.dir$//i if $^O eq 'VMS';
                
                my $dirpath = File::Spec->catdir( $dir, $file );

                print "# Deleting directory '$dirpath'\n" if $verbose;
                eval { rmtree( $dirpath ) };
                warn "Could not delete '$dirpath' while cleaning up '$dir'" 
                    if $@;
           
            ### regular file
            } else {
                print "# Deleting file '$path'\n" if $verbose;
                1 while unlink $path;
            }            
        }       
    
        close $dh;
    }
    
    return 1;
}
1;
