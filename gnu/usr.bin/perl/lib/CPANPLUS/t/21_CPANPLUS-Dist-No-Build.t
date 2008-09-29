### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;
use Test::More 'no_plan';

use CPANPLUS::Dist;
use CPANPLUS::Backend;
use CPANPLUS::Module::Fake;
use CPANPLUS::Module::Author::Fake;
use CPANPLUS::Internals::Constants;

my $Conf    = gimme_conf();
my $CB      = CPANPLUS::Backend->new( $Conf );

### set the config so that we will ignore the build installer,
### but prefer it anyway
{   CPANPLUS::Dist->_ignore_dist_types( INSTALLER_BUILD );
    $Conf->set_conf( prefer_makefile => 0 );
}

my $Mod = $CB->module_tree( 'Foo::Bar::MB::NOXS' );

ok( $Mod,                   "Module object retrieved" );        
ok( not grep { $_ eq INSTALLER_BUILD } CPANPLUS::Dist->dist_types,
                            "   Build installer not returned" );
            
### fetch the file first            
{   my $where = $Mod->fetch;
    ok( -e $where,          "   Tarball '$where' exists" );
}
    
### extract it, silence warnings/messages    
{   my $where = $Mod->extract;
    ok( -e $where,          "   Tarball extracted to '$where'" );
}

### check the installer type 
{   is( $Mod->status->installer_type, INSTALLER_MM, 
                            "Proper installer type found" );

    my $err = CPANPLUS::Error->stack_as_string;
    like( $err, '/'.INSTALLER_MM.'/',
                            "   Error mentions " . INSTALLER_MM );
    like( $err, '/'.INSTALLER_BUILD.'/',
                            "   Error mentions " . INSTALLER_BUILD );
    like( $err, qr/but might not be able to install/,
                            "   Error mentions install warning" );
}

END { 1 while unlink output_file()  }
