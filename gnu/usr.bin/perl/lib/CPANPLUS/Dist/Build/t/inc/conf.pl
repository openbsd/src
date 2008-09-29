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
    
    ### add our own path to the front of $ENV{PATH}, so that cpanp-run-perl
    ### and friends get picked up
    $old_env_path = $ENV{PATH};
    $ENV{'PATH'}  = join $Config{'path_sep'}, 
                    grep { defined } "$FindBin::Bin/../../../bin", $ENV{'PATH'};

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

### clean up files for PERLCORE mostly -- make clean isn't invoked
### there... otoh, we should clean up after ourselves anyway.
END {
    ### chdir to our own test dir, so we know all files are relative 
    ### to this point, no matter whether run from perlcore tests or
    ### regular CPAN installs
    chdir "$FindBin::Bin" if -d "$FindBin::Bin";

    ### XXX hardcoded
    _clean_test_dir( [qw|dummy-perl dummy-cpanplus| ] );
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
            
            ### John Malmberg reports yet another VMS issue:
            ### A directory name on VMS in VMS format ends with .dir 
            ### when it is referenced as a file.
            ### In UNIX format traditionally PERL on VMS does not remove the
            ### '.dir', however the VMS C library conversion routines do remove
            ### the '.dir' and the VMS C library routines can not handle the
            ### '.dir' being present on UNIX format filenames.
            ### So code doing the fixup has on VMS has to be able to handle both
            ### UNIX format names and VMS format names. 
            ### XXX See http://www.xray.mpe.mpg.de/
            ### mailing-lists/perl5-porters/2007-10/msg00064.html
            ### for details -- the below regex could use some touchups
            ### according to John. M.

            ### directory, rmtree it
            if( -d $path ) {

                $file =~ s/\.dir$//i if $^O eq 'VMS';

                ### Need a path specification here, not a file.
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
