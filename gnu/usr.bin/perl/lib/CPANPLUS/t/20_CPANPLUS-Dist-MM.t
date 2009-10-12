### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;

use CPANPLUS::Configure;
use CPANPLUS::Backend;
use CPANPLUS::Dist;
use CPANPLUS::Dist::MM;
use CPANPLUS::Internals::Constants;

use Test::More 'no_plan';
use Cwd;
use Config;
use Data::Dumper;
use File::Basename ();
use File::Spec ();

my $conf    = gimme_conf();
my $cb      = CPANPLUS::Backend->new( $conf );
my $File    = 'Bar.pm';

### if we need sudo that's no guarantee we can actually run it
### so set $noperms if sudo is required, as that may mean tests
### fail if you're not allowed to execute sudo. This resolves
### #29904: make test should not use sudo
my $noperms = $conf->get_program('sudo')        || #you need sudo
              $conf->get_conf('makemakerflags') || #you set some funky flags
              not -w $Config{installsitelib};      #cant write to install target

#$IPC::Cmd::DEBUG = $Verbose;

### Make sure we get the _EUMM_NOXS_ version
my $ModName = TEST_CONF_MODULE;

### This is the module name that gets /installed/
my $InstName = TEST_CONF_INST_MODULE;

### don't start sending test reports now... ###
$cb->_callbacks->send_test_report( sub { 0 } );
$conf->set_conf( cpantest => 0 );

### Redirect errors to file ###
*STDERR = output_handle() unless $conf->get_conf('verbose');

### dont uncomment this, it screws up where STDOUT goes and makes
### test::harness create test counter mismatches
#*STDOUT                          = output_handle() unless @ARGV;
### for the same test-output counter mismatch, we disable verbose
### mode
$conf->set_conf( allow_build_interactivity => 0 );

### start with fresh sources ###
ok( $cb->reload_indices( update_source => 0 ),
                                "Rebuilding trees" );

### we might need this Some Day when we're going to install into
### our own sandbox dir.. but for now, no dice due to EU::I bug
# $conf->set_program( sudo => '' );
# $conf->set_conf( makemakerflags => TEST_INSTALL_EU_MM_FLAGS );

### set alternate install dir ###
### XXX rather pointless, since we can't uninstall them, due to a bug
### in EU::Installed (6871). And therefor we can't test uninstall() or any of
### the EU::Installed functions. So, let's just install into sitelib... =/
#my $prefix  = File::Spec->rel2abs( File::Spec->catdir(cwd(),'dummy-perl') );
#my $rv = $cb->configure_object->set_conf( makemakerflags => "PREFIX=$prefix" );
#ok( $rv,                        "Alternate install path set" );

my $Mod     = $cb->module_tree( $ModName );
my $InstMod = $cb->module_tree( $InstName );
ok( $Mod,                       "Loaded object for: " . $Mod->name );
ok( $Mod,                       "Loaded object for: " . $InstMod->name );

### format_available tests ###
{   ok( CPANPLUS::Dist::MM->format_available,
                                "Format is available" );

    ### whitebox test!
    {   local $^W;
        local *CPANPLUS::Dist::MM::can_load = sub { 0 };
        ok(!CPANPLUS::Dist::MM->format_available,
                                "   Making format unavailable" );
    }

    ### test if the error got logged ok ###
    like( CPANPLUS::Error->stack_as_string,
          qr/You do not have .+?'CPANPLUS::Dist::MM' not available/s,
                                "   Format failure logged" );

    ### flush the stack ###
    CPANPLUS::Error->flush;
}

ok( $Mod->fetch,                "Fetching module to ".$Mod->status->fetch );
ok( $Mod->extract,              "Extracting module to ".$Mod->status->extract );

### test target => 'init'
{   my $dist = $Mod->dist( target => TARGET_INIT );
    ok( $dist,                  "Dist created with target => " . TARGET_INIT );
    ok( !$dist->status->prepared,
                                "   Prepare was not run" );
}                                

ok( $Mod->test,                 "Testing module" );

ok( $Mod->status->dist_cpan->status->test,
                                "   Test success registered as status" );
ok( $Mod->status->dist_cpan->status->prepared,
                                "   Prepared status registered" );
ok( $Mod->status->dist_cpan->status->created,
                                "   Created status registered" );
is( $Mod->status->dist_cpan->status->distdir, $Mod->status->extract,
                                "   Distdir status registered properly" );

### test the convenience methods
ok( $Mod->prepare,              "Preparing module" );
ok( $Mod->create,               "Creating module" );

ok( $Mod->dist,                 "Building distribution" );
ok( $Mod->status->dist_cpan,    "   Dist registered as status" );
isa_ok( $Mod->status->dist_cpan,    "CPANPLUS::Dist::MM" );

### flush the lib cache
### otherwise, cpanplus thinks the module's already installed
### since the blib is already in @INC
$cb->_flush( list => [qw|lib|] );

SKIP: {

    skip(q[No install tests under core perl],            10) if $ENV{PERL_CORE};
    skip(q[Possibly no permission to install, skipping], 10) if $noperms;

    ### we now say 'no perms' if sudo is configured, as per #29904
    #diag(q[Note: 'sudo' might ask for your password to do the install test])
    #    if $conf->get_program('sudo');

    ### make sure no options are set in PERL5_MM_OPT, as they might
    ### change the installation target and therefor will 1. mess up
    ### the tests and 2. leave an installed copy of our test module
    ### lying around. This addresses bug #29716: 20_CPANPLUS-Dist-MM.t 
    ### fails (and leaves test files installed) when EUMM options 
    ### include INSTALL_BASE
    {   local $ENV{'PERL5_MM_OPT'};
    
        ### add the new dir to the configuration too, so eu::installed tests
        ### work as they should
        $conf->set_conf( lib => [ TEST_CONF_INSTALL_DIR ] );
    
        ok( $Mod->install(  force           => 1, 
                            makemakerflags  => 'PREFIX='.TEST_CONF_INSTALL_DIR, 
                        ),      "Installing module" );
    }                                
                                
    ok( $Mod->status->installed,"   Module installed according to status" );


    SKIP: {   ### EU::Installed tests ###
        ### EU::I sometimes fails. See:
        ### #43292: ~/CPANPLUS-0.85_04 fails t/20_CPANPLUS-Dist-MM.t
        ### #46890: ExtUtils::Installed + EU::MM PREFIX= don't always work
        ### well together
        skip( "ExtUtils::Installed issue #46890 prevents these tests from running reliably", 8 );
    
    
        skip( "Old perl on cygwin detected " .
              "-- tests will fail due to known bugs", 8
        ) if ON_OLD_CYGWIN;

        ### might need it Later when EU::I is fixed..
        #local @INC = ( TEST_INSTALL_DIR_LIB, @INC );

        {   ### validate
            my @missing = $InstMod->validate;

            is_deeply( \@missing, [],
                                    "No missing files" );
        }

        {   ### files
            my @files = $InstMod->files;

            ### number of files may vary from OS to OS
            ok( scalar(@files),     "All files accounted for" );
            ok( grep( /$File/, @files),
                                    "   Found the module" );

            ### XXX does this work on all OSs?
            #ok( grep( /man/, @files ),
            #                        "   Found the manpage" );
        }

        {   ### packlist
            my ($obj) = $InstMod->packlist;
            isa_ok( $obj,           "ExtUtils::Packlist" );
        }

        {   ### directory_tree
            my @dirs = $InstMod->directory_tree;
            ok( scalar(@dirs),      "Directory tree obtained" );

            my $found;
            for my $dir (@dirs) {
                ok( -d $dir,        "   Directory exists" );

                my $file = File::Spec->catfile( $dir, $File );
                $found = $file if -e $file;
            }

            ok( -e $found,          "   Module found" );
        }

        SKIP: {
            skip("Probably no permissions to uninstall", 1)
                if $noperms;

            ok( $InstMod->uninstall,"Uninstalling module" );
        }
    }
}

### test exceptions in Dist::MM->create ###
{   ok( $Mod->status->mk_flush, "Old status info flushed" );
    my $dist = INSTALLER_MM->new( module => $Mod );
    
    ok( $dist,                  "New dist object made" );
    ok(!$dist->prepare,         "   Dist->prepare failed" );
    like( CPANPLUS::Error->stack_as_string, qr/No dir found to operate on/,
                                "       Failure logged" );

    ### manually set the extract dir,
    $Mod->status->extract($0);

    ok(!$dist->create,          "   Dist->create failed" );
    like( CPANPLUS::Error->stack_as_string, qr/not successfully prepared/s,
                                "       Failure logged" );

    ### pretend we've been prepared ###
    $dist->status->prepared(1);

    ok(!$dist->create,          "   Dist->create failed" );
    like( CPANPLUS::Error->stack_as_string, qr/Could not chdir/s,
                                "       Failure logged" );
}

### writemakefile.pl tests ###
{   ### remove old status info
    ok( $Mod->status->mk_flush, "Old status info flushed" );
    ok( $Mod->fetch,            "Module fetched again" );
    ok( $Mod->extract,          "Module extracted again" );

    ### cheat and add fake prereqs ###
    my $prereq = TEST_CONF_PREREQ;

    $Mod->status->prereqs( { $prereq => 0 } );

    my $makefile_pl = MAKEFILE_PL->( $Mod->status->extract );
    my $makefile    = MAKEFILE->(    $Mod->status->extract );

    my $dist        = $Mod->dist;
    ok( $dist,                  "Dist object built" );

    ### check for a makefile.pl and 'write' one
    ok( -s $makefile_pl,        "   Makefile.PL present" );
    ok( $dist->write_makefile_pl( force => 0 ),
                                "   Makefile.PL written" );
    like( CPANPLUS::Error->stack_as_string, qr/Already created/,
                                "   Prior existance noted" );

    ### ok, unlink the makefile.pl, now really write one
    1 while unlink $makefile;

    ### must do '1 while' for VMS
    {   my $unlink_sts = unlink($makefile_pl);
        1 while unlink $makefile_pl;
        ok( $unlink_sts,        "Deleting Makefile.PL");
    }

    ok( !-s $makefile_pl,       "   Makefile.PL deleted" );
    ok( !-s $makefile,          "   Makefile deleted" );
    ok($dist->write_makefile_pl,"   Makefile.PL written" );

    ### see if we wrote anything sensible
    my $fh = OPEN_FILE->( $makefile_pl );
    ok( $fh,                    "Makefile.PL open for read" );

    my $str = do { local $/; <$fh> };
    like( $str, qr/### Auto-generated .+ by CPANPLUS ###/,
                                "   Autogeneration noted" );
    like( $str, '/'. $Mod->module .'/',
                                "   Contains module name" );
    like( $str, '/'. quotemeta($Mod->version) . '/',
                                "   Contains version" );
    like( $str, '/'. $Mod->author->author .'/',
                                "   Contains author" );
    like( $str, '/PREREQ_PM/',  "   Contains prereqs" );
    like( $str, qr/$prereq.+0/, "   Contains prereqs" );

    close $fh;

    ### seems ok, now delete it again and go via install()
    ### to see if it picks up on the missing makefile.pl and
    ### does the right thing
    ### must do '1 while' for VMS
    {   my $unlink_sts = unlink($makefile_pl);
        1 while unlink $makefile_pl;
        ok( $unlink_sts,        "Deleting Makefile.PL");
    }    
    ok( !-s $makefile_pl,       "   Makefile.PL deleted" );
    ok( $dist->status->mk_flush,"Dist status flushed" );
    ok( $dist->prepare,         "   Dist->prepare run again" );
    ok( $dist->create,          "   Dist->create run again" );
    ok( -s $makefile_pl,        "   Makefile.PL present" );
    like( CPANPLUS::Error->stack_as_string,
          qr/attempting to generate one/,
                                "   Makefile.PL generation attempt logged" );

    ### now let's throw away the makefile.pl, flush the status and not
    ### write a makefile.pl
    {   local $^W;
        local *CPANPLUS::Dist::MM::write_makefile_pl = sub { 1 };

        1 while unlink $makefile_pl;
        1 while unlink $makefile;

        ok(!-s $makefile_pl,        "Makefile.PL deleted" );
        ok(!-s $makefile,           "Makefile deleted" );
        ok( $dist->status->mk_flush,"Dist status flushed" );
        ok(!$dist->prepare,         "   Dist->prepare failed" );
        like( CPANPLUS::Error->stack_as_string,
              qr/Could not find 'Makefile.PL'/i,
                                    "   Missing Makefile.PL noted" );
        is( $dist->status->makefile, 0,
                                    "   Did not manage to create Makefile" );
    }

    ### now let's write a makefile.pl that just does 'die'
    {   local $^W;
        local *CPANPLUS::Dist::MM::write_makefile_pl = 
            __PACKAGE__->_custom_makefile_pl_sub( "exit 1;" );

        ### there's no makefile.pl now, since the previous test failed
        ### to create one
        #ok( -e $makefile_pl,        "Makefile.PL exists" );
        #ok( unlink($makefile_pl),   "   Deleting Makefile.PL");
        ok(!-s $makefile_pl,        "Makefile.PL deleted" );
        ok( $dist->status->mk_flush,"Dist status flushed" );
        ok(!$dist->prepare,         "   Dist->prepare failed" );
        like( CPANPLUS::Error->stack_as_string, qr/Could not run/s,
                                    "   Logged failed 'perl Makefile.PL'" );
        is( $dist->status->makefile, 0,
                                    "   Did not manage to create Makefile" );
    }

    ### clean up afterwards ###
    ### must do '1 while' for VMS
    {   my $unlink_sts = unlink($makefile_pl);
        1 while unlink $makefile_pl;
        ok( $unlink_sts,        "Deleting Makefile.PL");
    }   
    
    $dist->status->mk_flush;
}

### test ENV setting in Makefile.PL
{   ### use print() not die() -- we're redirecting STDERR in tests!
    my $env     = ENV_CPANPLUS_IS_EXECUTING;
    my $sub     = __PACKAGE__->_custom_makefile_pl_sub(
                                    "print qq[ENV=\$ENV{$env}\n]; exit 1;" );
    
    my $clone   = $Mod->clone;
    $clone->status->fetch( $Mod->status->fetch );
    
    ok( $clone,                 'Testing ENV settings $dist->prepare' );
    ok( $clone->extract,        '   Files extracted' );
    ok( $clone->prepare,        '   $mod->prepare worked first time' );
    
    my $dist        = $clone->status->dist;
    my $makefile_pl = MAKEFILE_PL->( $clone->status->extract );

    ok( $sub->($dist),          "   Custom Makefile.PL written" );
    ok( -e $makefile_pl,        "       File exists" );

    ### clear errors    
    CPANPLUS::Error->flush;

    my $rv = $dist->prepare( force => 1, verbose => 0 );
    ok( !$rv,                   '   $dist->prepare failed' );

    SKIP: {
        skip( "Can't test ENV{$env} -- no buffers available", 1 )
            unless IPC::Cmd->can_capture_buffer;

        my $re = quotemeta( $makefile_pl );
        like( CPANPLUS::Error->stack_as_string, qr/ENV=$re/,
                                "   \$ENV $env set correctly during execution");
    }

    ### and the ENV var should no longer be set now
    ok( !$ENV{$env},            "   ENV var now unset" );
}    

sub _custom_makefile_pl_sub {
    my $pkg = shift;
    my $txt = shift or return;
    
    return sub {
        my $dist = shift; 
        my $self = $dist->parent;
        my $fh   = OPEN_FILE->(
                    MAKEFILE_PL->($self->status->extract), '>' );
        print $fh $txt;
        close $fh;
    
        return 1;
    }
}


# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:


