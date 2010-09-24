### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;

use CPANPLUS::Backend;

use Test::More 'no_plan';
use Data::Dumper;
use File::Spec;
use Cwd;
use File::Basename;
use CPANPLUS::Internals::Constants;

my $conf = gimme_conf();

my $cb = CPANPLUS::Backend->new( $conf );
isa_ok($cb, "CPANPLUS::Internals" );

my $mod = $cb->module_tree( TEST_CONF_MODULE );
isa_ok( $mod,  'CPANPLUS::Module' );

### fail host tests ###
{   my $host = {};
    my $rv   = $cb->_add_fail_host( host => $host );
    
    ok( $rv,                    "Failed host added " );
    ok(!$cb->_host_ok( host => $host),   
                                "   Host registered as failed" );
    ok( $cb->_host_ok( host => {} ),    
                                "   Fresh host unregistered" );
}

### refetch, even if it's there already ###
{   my $where = $cb->_fetch( module => $mod, force => 1 );

    ok( $where,                 "File downloaded to '$where'" );
    ok( -s $where,              "   File exists" );                          
    unlink $where;
    ok(!-e $where,              "   File removed" );
}

### try to fetch something that doesn't exist ###
{   ### set up a bogus host first ###
    my $hosts   = $conf->get_conf('hosts');
    my $fail    = { scheme  => 'file', 
                    path    => "$0/$0" };
    
    unshift @$hosts, $fail;
    $conf->set_conf( hosts => $hosts );
    
    ### the fallback host will get it ###
    my $where = $cb->_fetch( module => $mod, force => 1, verbose => 0 );
    ok($where,                  "File downloaded to '$where'" );
    ok( -s $where,              "   File exists" );                          
    
    ### but the error should be recorded ###
    like( CPANPLUS::Error->stack_as_string, qr/Fetching of .*? failed/s,
                                "   Error recorded appropriately" ); 

    ### host marked as bad? ###
    ok(!$cb->_host_ok( host => $fail ),   
                                "   Failed host logged properly" );    

    ### restore the hosts ###
    shift @$hosts; $conf->set_conf( hosts => $hosts );
}

### try and fetch a URI
{   my $base    = basename($0);

    ### do an ON_UNIX test, cygwin will fail tests otherwise (#14553)
    ### create a file URI. Make sure to split it by LOCAL rules
    ### and JOIN by unix rules, so we get a proper file uri
    ### otherwise, we might break win32. See bug #18702
    my $cwd     = cwd();
    my $in_file = $^O eq 'VMS'
        ? VMS::Filespec::unixify( File::Spec->catfile($cwd, $base) )
        : File::Spec::Unix->catfile(
              File::Spec::Unix->catdir( File::Spec->splitdir( $cwd ) ),
              $base
          ); 
          
    my $target  = CREATE_FILE_URI->($in_file);

    my $fake    = $cb->parse_module( module => $target );
    
    ok( IS_FAKE_MODOBJ->(mod => $fake), 
                                "Fake module created from $0" );
    is( $fake->status->_fetch_from, $target,
                                "   Fetch from set ok" );                                 
                                
    my $where = $fake->fetch;
    ok( $where,                 "   $target fetched ok" );
    ok( -s $where,              "   $where exists" );
    like( $where, '/'. UNKNOWN_DL_LOCATION .'/',
                                "   Saved to proper location" );
    like( $where, qr/$base$/,   "   Saved with proper name" );                                
}


# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
