### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;

use CPANPLUS::Configure;
use CPANPLUS::Backend;
use CPANPLUS::Internals::Constants;
use Test::More 'no_plan';
use Data::Dumper;

my $conf = gimme_conf();

my $cb = CPANPLUS::Backend->new( $conf );

### XXX SOURCEFILES FIX
my $mod     = $cb->module_tree( TEST_CONF_MODULE );

isa_ok( $mod,  'CPANPLUS::Module' );

my $where = $mod->fetch;
ok( $where,             "Module fetched" );

my $dir = $cb->_extract( module => $mod );
ok( $dir,               "Module extracted" );
ok( DIR_EXISTS->($dir), "   Dir exists" );

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
