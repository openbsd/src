### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;
use Test::More 'no_plan';
use Cwd;
use Config;
use File::Basename;

use CPANPLUS::Internals::Constants;
use CPANPLUS::Module::Fake;
use CPANPLUS::Module::Author::Fake;
use CPANPLUS::Configure;
use CPANPLUS::Backend;

my $conf = gimme_conf();

my $cb = CPANPLUS::Backend->new( $conf );

my $f_auth = CPANPLUS::Module::Author::Fake->new( _id => $cb->_id );
ok( $f_auth,                        "Fake auth object created" );
ok( IS_AUTHOBJ->( $f_auth ),        "   IS_AUTHOBJ recognizes it" );
ok( IS_FAKE_AUTHOBJ->( $f_auth ),   "   IS_FAKE_AUTHOBJ recognizes it" );

my $f_mod = CPANPLUS::Module::Fake->new(
                module  => TEST_CONF_INST_MODULE ,
                path    => 'some/where',
                package => 'Foo-Bar-1.2.tgz',
                _id     => $cb->_id,
            );
ok( $f_mod,                     "Fake mod object created" );
ok( IS_MODOBJ->( $f_mod ),      "   IS_MODOBJ recognizes it" );
ok( IS_FAKE_MODOBJ->( $f_mod ), "   IS_FAKE_MODOJB recognizes it" );

ok( IS_CONFOBJ->( conf => $conf ),  "IS_CONFOBJ recognizes conf object" );

ok( FILE_EXISTS->( file => basename($0) ),      "FILE_EXISTS finds file" );
ok( FILE_READABLE->( file => basename($0) ),    "FILE_READABLE finds file" );
ok( DIR_EXISTS->( dir => cwd() ),               "DIR_EXISTS finds dir" );
            

{   no strict 'refs';

    my $tmpl = {
        MAKEFILE_PL => 'Makefile.PL',
        BUILD_PL    => 'Build.PL',
        BLIB        => 'blib',
        MAKEFILE    => do {
            ### On vms, it's a different name. See constants
            ### file for details
            (ON_VMS and $Config::Config{make} =~ /MM[S|K]/i)
                ? 'DESCRIP.MMS'
                : 'Makefile'
        },
    };
    
    while ( my($sub,$res) = each %$tmpl ) {
        is( &{$sub}->(), $res, "$sub returns proper result without args" );
        
        my $long = File::Spec->catfile( cwd(), $res );
        is( &{$sub}->( cwd() ), $long, "$sub returns proper result with args" );
    }       
}                               
      
# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:          
