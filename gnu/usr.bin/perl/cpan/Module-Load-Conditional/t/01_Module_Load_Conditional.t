### Module::Load::Conditional test suite ###
### this should no longer be needed
# BEGIN { 
#     if( $ENV{PERL_CORE} ) {
#         chdir '../lib/Module/Load/Conditional' 
#             if -d '../lib/Module/Load/Conditional';
#         unshift @INC, '../../../..';
#     
#         ### fix perl location too
#         $^X = '../../../../../t/' . $^X;
#     }
# } 

BEGIN { use FindBin; }
BEGIN { chdir 't' if -d 't' }

use strict;
use File::Spec ();
use Test::More 'no_plan';

use constant ON_VMS     => $^O eq 'VMS';

use lib File::Spec->catdir($FindBin::Bin, qw[.. lib] );
use lib File::Spec->catdir($FindBin::Bin, q[to_load] );

use_ok( 'Module::Load::Conditional' );

### stupid stupid warnings ###
{   $Module::Load::Conditional::VERBOSE =   
    $Module::Load::Conditional::VERBOSE = 0;

    *can_load       = *Module::Load::Conditional::can_load
                    = *Module::Load::Conditional::can_load;
    *check_install  = *Module::Load::Conditional::check_install
                    = *Module::Load::Conditional::check_install;
    *requires       = *Module::Load::Conditional::requires
                    = *Module::Load::Conditional::requires;
}

{
    my $rv = check_install(
                        module  => 'Module::Load::Conditional',
                        version => $Module::Load::Conditional::VERSION,
                );

    ok( $rv->{uptodate},    q[Verify self] );
    is( $rv->{version}, $Module::Load::Conditional::VERSION,  
                            q[  Found proper version] );
    ok( $rv->{dir},         q[  Found directory information] );
    
    {   my $dir = File::Spec->canonpath( $rv->{dir} );

        ### special rules apply on VMS, as always...
        if (ON_VMS) {
            ### Need path syntax for VMS compares.
            $dir = VMS::Filespec::pathify($dir);
            ### Remove the trailing VMS specific directory delimiter
            $dir =~ s/\]//;
        }    
    
        ### quote for Win32 paths, use | to avoid slash confusion
        my $dir_re = qr|^\Q$dir\E|i;
        like( File::Spec->canonpath( $rv->{file} ), $dir_re,
                            q[      Dir subset of file path] );
    }

    ### break up the specification
    my @rv_path = do {

        ### Use the UNIX specific method, as the VMS one currently
        ### converts the file spec back to VMS format.
        my $class = ON_VMS ? 'File::Spec::Unix' : 'File::Spec';
        
        my($vol, $path, $file) = $class->splitpath( $rv->{'file'} );

        my @path = ($vol, $class->splitdir( $path ), $file );

        ### First element could be blank for some system types like VMS
        shift @path if $vol eq '';

        ### and return it    
        @path;
    };
    my $inc_path = $INC{'Module/Load/Conditional.pm'};
    if ( $^O eq 'MSWin32' ) {
        $inc_path = File::Spec->canonpath( $inc_path );
        $inc_path =~ s{\\}{/}g; # to meet with unix path
    }
    is( $inc_path,
            File::Spec::Unix->catfile(@rv_path),
                            q[  Found proper file]
    );
    
    

}

### the version may contain an _, which means perl will warn about 'not
### numeric' -- turn off that warning here.
{   local $^W;
    my $rv = check_install(
                        module  => 'Module::Load::Conditional',
                        version => $Module::Load::Conditional::VERSION + 1,
                );

    ok( !$rv->{uptodate} && $rv->{version} && $rv->{file},
        q[Verify out of date module]
    );
}

{
    my $rv = check_install( module  => 'Module::Load::Conditional' );

    ok( $rv->{uptodate} && $rv->{version} && $rv->{file},
        q[Verify any module]
    );
}

{
    my $rv = check_install( module  => 'Module::Does::Not::Exist' );

    ok( !$rv->{uptodate} && !$rv->{version} && !$rv->{file},
        q[Verify non-existant module]
    );

}

### test finding a version of a module that mentions $VERSION in pod
{   my $rv = check_install( module => 'InPod' );
    ok( $rv,                        'Testing $VERSION in POD' );
    ok( $rv->{version},             "   Version found" );
    is( $rv->{version}, 2,          "   Version is correct" );
}

### test beta/developer release versions
{   my $test_ver = $Module::Load::Conditional::VERSION;
    
    ### strip beta tags
    $test_ver =~ s/_\d+//g;
    $test_ver .= '_99';
    
    my $rv = check_install( 
                    module  => 'Module::Load::Conditional', 
                    version => $test_ver,
                );

    ok( $rv,                "Checking beta versions" );
    ok( !$rv->{'uptodate'}, "   Beta version is higher" );
    
}    

### test $FIND_VERSION
{   local $Module::Load::Conditional::FIND_VERSION = 0;
    local $Module::Load::Conditional::FIND_VERSION = 0;
    
    my $rv = check_install( module  => 'Module::Load::Conditional' );

    ok( $rv,                        'Testing $FIND_VERSION' );
    is( $rv->{version}, undef,      "   No version info returned" );
    ok( $rv->{uptodate},            "   Module marked as uptodate" );
}    

### test 'can_load' ###

{
    my $use_list = { 'LoadIt' => 1 };
    my $bool = can_load( modules => $use_list );

    ok( $bool, q[Load simple module] );
}

{
    my $use_list = { 'Commented' => 2 };
    my $bool = can_load( modules => $use_list );

    ok( $bool, q[Load module with a second, commented-out $VERSION] );
}

{
    my $use_list = { 'MustBe::Loaded' => 1 };
    my $bool = can_load( modules => $use_list );

    ok( !$bool, q[Detect out of date module] );
}

{
    delete $INC{'LoadIt.pm'};
    delete $INC{'MustBe/Loaded.pm'};

    my $use_list = { 'LoadIt' => 1, 'MustBe::Loaded' => 1 };
    my $bool = can_load( modules => $use_list );

    ok( !$INC{'LoadIt.pm'} && !$INC{'MustBe/Loaded.pm'},
        q[Do not load if one prerequisite fails]
    );
}


### test 'requires' ###
SKIP:{
    skip "Depends on \$^X, which doesn't work well when testing the Perl core", 
        1 if $ENV{PERL_CORE};

    my %list = map { $_ => 1 } requires('Carp');
    
    my $flag;
    $flag++ unless delete $list{'Exporter'};

    ok( !$flag, q[Detecting requirements] );
}

### test using the %INC lookup for check_install
{   local $Module::Load::Conditional::CHECK_INC_HASH = 1;
    local $Module::Load::Conditional::CHECK_INC_HASH = 1;
    
    {   package A::B::C::D; 
        $A::B::C::D::VERSION = $$; 
        $INC{'A/B/C/D.pm'}   = $$.$$;
        
        ### XXX this is no longer needed with M::Load 0.11_01
        #$INC{'[.A.B.C]D.pm'} = $$.$$ if $^O eq 'VMS';
    }
    
    my $href = check_install( module => 'A::B::C::D', version => 0 );

    ok( $href,                  'Found package in %INC' );
    is( $href->{'file'}, $$.$$, '   Found correct file' );
    is( $href->{'version'}, $$, '   Found correct version' );
    ok( $href->{'uptodate'},    '   Marked as uptodate' );
    ok( can_load( modules => { 'A::B::C::D' => 0 } ),
                                '   can_load successful' );
}

