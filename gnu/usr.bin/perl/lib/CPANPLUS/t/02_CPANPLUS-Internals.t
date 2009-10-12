### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;
use Test::More 'no_plan';

use CPANPLUS::Configure;
use CPANPLUS::Backend;
use CPANPLUS::Internals::Constants;
use Module::Load::Conditional       qw[can_load];
use Data::Dumper;

my $cb = CPANPLUS::Backend->new( CPANPLUS::Configure->new() );

isa_ok($cb,                 'CPANPLUS::Internals');
is($cb->_id, $cb->_last_id, "Comparing ID's");

### delete/store/retrieve id tests ###
{   my $del = $cb->_remove_id( $cb->_id );
    ok( $del,                   "ID deleted" );
    isa_ok( $del,               "CPANPLUS::Internals" );
    is( $del, $cb,              "   Deleted ID matches last object" );
    
    my $id = $cb->_store_id( $del );
    ok( $id,                    "ID stored" );
    is( $id, $cb->_id,          "   Stored proper ID" );
    
    my $obj = $cb->_retrieve_id( $id );
    ok( $obj,                   "Object retrieved from ID" );
    isa_ok( $obj,               'CPANPLUS::Internals' );
    is( $obj->_id, $id,         "   Retrieved ID properly" );
    
    my @obs = $cb->_return_all_objects();
    ok( scalar(@obs),           "Returned objects" );
    is( scalar(@obs), 1,        "   Proper amount of objects found" );
    is( $obs[0]->_id, $id,      "   Proper ID found on object" );
    
    my $lid = $cb->_last_id;
    ok( $lid,                   "Found last registered ID" );
    is( $lid, $id,              "   ID matches last object" );

    my $iid = $cb->_inc_id;
    ok( $iid,                   "Incremented ID" );
    is( $iid, $id+1,            "   ID matched last ID + 1" );
}    

### host ok test ###
{
    my $host = $cb->configure_object->get_conf('hosts')->[0];
    
    is( $cb->_host_ok( host => $host ),     1,  "Host ok" );
    is( $cb->_add_fail_host(host => $host), 1,  "   Host now marked as bad" );
    is( $cb->_host_ok( host => $host ),     0,  "   Host still bad" );
    ok( $cb->_flush( list => ['hosts'] ),       "   Hosts flushed" );
    is( $cb->_host_ok( host => $host ),     1,  "   Host now ok again" );
}    

### flush loads test
{   my $mod     = 'Benchmark';
    my $file    = $mod . '.pm';
    
    ### XXX whitebox test -- mark this module as unloadable
    $Module::Load::Conditional::CACHE->{$mod}->{usable} = 0;

    ok( !can_load( modules => { $mod => 0 }, verbose => 0 ),
                                                "'$mod' not loaded" );
                                                
    ok( $cb->flush('load'),                     "   'load' cache flushed" );
    ok( can_load( modules => { $mod => 0 }, verbose => 0 ),
                                                "   '$mod' loaded" );
}

### add to inc path tests
{   my $meth = '_add_to_includepath';
    can_ok( $cb,                $meth );
    
    my $p5lib   = $ENV{PERL5LIB} || '';
    my $inc     = "@INC";         
    ok( $cb->$meth( directories => [$$] ),    
                                "   CB->$meth( $$ )" );
    
    my $new_p5lib   = $ENV{PERL5LIB};
    my $new_inc     = "@INC";    
    isnt( $p5lib, $new_p5lib,   "       PERL5LIB is now: $new_p5lib" );
    like( $new_p5lib, qr/$$/,   "           Matches $$" );

    isnt( $inc, $new_inc,       '       @INC is expanded with: ' . $$ );
    like( $new_inc, qr/$$/,     "           Matches $$" );
    
    ok( $cb->$meth( directories => [$$] ),    
                                "       CB->$meth( $$ ) again" );
    is( "@INC", $new_inc,       '           @INC unchanged' );
    is( $new_p5lib, $ENV{PERL5LIB},
                                "           PERL5LIB unchanged" );
}    

### callback registering tests ###
{    my $callback_map = {
        ### name                default value    
        install_prerequisite    => 1,   # install prereqs when 'ask' is set?
        edit_test_report        => 0,   # edit the prepared test report?
        send_test_report        => 1,   # send the test report?
        munge_test_report       => $$,  # munge the test report
        filter_prereqs          => $$,  # limit prereqs
        proceed_on_test_failure => 0,   # continue on failed 'make test'?
        munge_dist_metafile     => $$,  # munge the metailfe
    };

    for my $callback ( keys %$callback_map ) {
        
        {   my $rv = $callback_map->{$callback};

            is( $rv, $cb->_callbacks->$callback->( $0, $$ ),
                                "Default callback '$callback' called" );
            like( CPANPLUS::Error->stack_as_string, qr/DEFAULT '\S+' HANDLER/s,  
                                "   Default handler warning recorded" );       
            CPANPLUS::Error->flush;
        }
        
        ### try to register the callback
        my $ok = $cb->_register_callback(
                        name    => $callback,
                        code    => sub { return $callback }
                    );
                    
        ok( $ok,                "Registered callback '$callback' ok" );
        
        my $sub = $cb->_callbacks->$callback;
        ok( $sub,               "   Retrieved callback" );
        ok( IS_CODEREF->($sub), "   Callback is a sub" );
        
        my $rv = $sub->();
        ok( $rv,                "   Callback called ok" );
        is( $rv, $callback,     "   Got expected return value" );
    }   
}


# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
