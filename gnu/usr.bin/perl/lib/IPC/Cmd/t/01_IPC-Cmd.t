## IPC::Cmd test suite ###

BEGIN { chdir 't' if -d 't' };

use strict;
use lib qw[../lib];
use File::Spec ();
use Test::More 'no_plan';

my $Class   = 'IPC::Cmd';
my @Funcs   = qw[run can_run];
my @Meths   = qw[can_use_ipc_run can_use_ipc_open3 can_capture_buffer];
my $IsWin32 = $^O eq 'MSWin32';
my $Verbose = @ARGV ? 1 : 0;

use_ok( $Class,         $_ ) for @Funcs;
can_ok( $Class,         $_ ) for @Funcs, @Meths;
can_ok( __PACKAGE__,    $_ ) for @Funcs;

my $Have_IPC_Run    = $Class->can_use_ipc_run;
my $Have_IPC_Open3  = $Class->can_use_ipc_open3;

$IPC::Cmd::VERBOSE  = $IPC::Cmd::VERBOSE = $Verbose;

### run tests in various configurations, based on what modules we have
my @Prefs = ( 
    [ $Have_IPC_Run, $Have_IPC_Open3 ], 
    [ 0,             $Have_IPC_Open3 ], 
    [ 0,             0 ] 
);

### can_run tests
{
    ok( can_run('perl'),                q[Found 'perl' in your path] );
    ok( !can_run('10283lkjfdalskfjaf'), q[Not found non-existant binary] );
}

### run tests that print only to stdout
{   ### list of commands and regexes matching output ###
    my $map = [
        # command                                    # output regex
        [ "$^X -v",                                  qr/larry\s+wall/i, ],
        [ [$^X, '-v'],                               qr/larry\s+wall/i, ],
        [ "$^X -eprint+42 | $^X -neprint",           qr/42/,            ],
        [ [$^X,qw[-eprint+42 |], $^X, qw|-neprint|], qr/42/,            ],
    ];

    diag( "Running tests that print only to stdout" ) if $Verbose;
    ### for each configuarion
    for my $pref ( @Prefs ) {
        diag( "Running config: IPC::Run: $pref->[0] IPC::Open3: $pref->[1]" )
            if $Verbose;

        $IPC::Cmd::USE_IPC_RUN    = $IPC::Cmd::USE_IPC_RUN      = $pref->[0];
        $IPC::Cmd::USE_IPC_OPEN3  = $IPC::Cmd::USE_IPC_OPEN3    = $pref->[1];

        ### for each command
        for my $aref ( @$map ) {
            my $cmd                 = $aref->[0];
            my $regex               = $aref->[1];

            my $pp_cmd = ref $cmd ? "@$cmd" : "$cmd";
            diag( "Running '$pp_cmd' as " . (ref $cmd ? "ARRAY" : "SCALAR") ) 
                if $Verbose;

            ### in scalar mode
            {   diag( "Running scalar mode" ) if $Verbose;
                my $buffer;
                my $ok = run( command => $cmd, buffer => \$buffer );

                ok( $ok,        "Ran command succesfully" );
                
                SKIP: {
                    skip "No buffers available", 1 
                                unless $Class->can_capture_buffer;
                    
                    like( $buffer, $regex,  
                                "   Buffer filled properly" );
                }
            }
                
            ### in list mode                
            {   diag( "Running list mode" ) if $Verbose;
                my @list = run( command => $cmd );
                ok( $list[0],   "Command ran successfully" );
                ok( !$list[1],  "   No error code set" );

                my $list_length = $Class->can_capture_buffer ? 5 : 2;
                is( scalar(@list), $list_length,
                                "   Output list has $list_length entries" );

                SKIP: {
                    skip "No buffers available", 6 
                                unless $Class->can_capture_buffer;
                    
                    ### the last 3 entries from the RV, are they array refs?
                    isa_ok( $list[$_], 'ARRAY' ) for 2..4;

                    like( "@{$list[2]}", $regex,
                                "   Combined buffer holds output" );

                    like( "@{$list[3]}", qr/$regex/,
                            "   Stdout buffer filled" );
                    is( scalar( @{$list[4]} ), 0,
                                    "   Stderr buffer empty" );
                }
            }
        }
    }
}

### run tests that print only to stderr
### XXX lots of duplication from stdout tests, only difference
### is buffer inspection
{   ### list of commands and regexes matching output ###
    my $map = [
        # command                                    # output regex
        [ "$^X -ewarn+42",                          qr/^42 /, ],
        [ [$^X, '-ewarn+42'],                       qr/^42 /, ],
    ];

    diag( "Running tests that print only to stderr" ) if $Verbose;
    ### for each configuarion
    for my $pref ( @Prefs ) {
        diag( "Running config: IPC::Run: $pref->[0] IPC::Open3: $pref->[1]" )
            if $Verbose;

        $IPC::Cmd::USE_IPC_RUN    = $IPC::Cmd::USE_IPC_RUN      = $pref->[0];
        $IPC::Cmd::USE_IPC_OPEN3  = $IPC::Cmd::USE_IPC_OPEN3    = $pref->[1];

        ### for each command
        for my $aref ( @$map ) {
            my $cmd                 = $aref->[0];
            my $regex               = $aref->[1];

            my $pp_cmd = ref $cmd ? "@$cmd" : "$cmd";
            diag( "Running '$pp_cmd' as " . (ref $cmd ? "ARRAY" : "SCALAR") )
                if $Verbose;

            ### in scalar mode
            {   diag( "Running stderr command in scalar mode" ) if $Verbose;
                my $buffer;
                my $ok = run( command => $cmd, buffer => \$buffer );

                ok( $ok,        "Ran stderr command succesfully in scalar mode." );

                SKIP: {
           # No buffers are expected if neither IPC::Run nor IPC::Open3 is used.
                    skip "No buffers available", 1
                                unless $Class->can_capture_buffer;

                    like( $buffer, $regex,
                                "   Buffer filled properly from stderr" );
                }
            }

            ### in list mode
            {   diag( "Running stderr command in list mode" ) if $Verbose;
                my @list = run( command => $cmd );
                ok( $list[0],   "Ran stderr command successfully in list mode." );
                ok( !$list[1],  "   No error code set" );

                my $list_length = $Class->can_capture_buffer ? 5 : 2;
                is( scalar(@list), $list_length,
                                "   Output list has $list_length entries" );

                SKIP: {
           # No buffers are expected if neither IPC::Run nor IPC::Open3 is used.
                    skip "No buffers available", 6
                                unless $Class->can_capture_buffer;

                    ### the last 3 entries from the RV, are they array refs?
                    isa_ok( $list[$_], 'ARRAY' ) for 2..4;

                    like( "@{$list[2]}", $regex,
                                "   Combined buffer holds output" );

                    is( scalar( @{$list[3]} ), 0,
                                    "   Stdout buffer empty" );
                    like( "@{$list[4]}", qr/$regex/,
                            "   Stderr buffer filled" );
                }
            }
        }
    }
}

### test failures
{   ### for each configuarion
    for my $pref ( @Prefs ) {
        diag( "Running config: IPC::Run: $pref->[0] IPC::Open3: $pref->[1]" )
            if $Verbose;

        $IPC::Cmd::USE_IPC_RUN    = $IPC::Cmd::USE_IPC_RUN      = $pref->[0];
        $IPC::Cmd::USE_IPC_OPEN3  = $IPC::Cmd::USE_IPC_OPEN3    = $pref->[1];

        my $ok = run( command => "$^X -ledie" );
        ok( !$ok,               "Failure caught" );
    }
}    

__END__


### check if IPC::Run is already loaded, if so, IPC::Run tests
### from IPC::Run are known to fail on win32
my $Skip_IPC_Run = ($^O eq 'MSWin32' && exists $INC{'IPC/Run.pm'}) ? 1 : 0;

use_ok( 'IPC::Cmd' ) or diag "Cmd.pm not found.  Dying", die;

IPC::Cmd->import( qw[can_run run] );

### silence it ###
$IPC::Cmd::VERBOSE = $IPC::Cmd::VERBOSE = $ARGV[0] ? 1 : 0;

{
    ok( can_run('perl'),                q[Found 'perl' in your path] );
    ok( !can_run('10283lkjfdalskfjaf'), q[Not found non-existant binary] );
}


{   ### list of commands and regexes matching output ###
    my $map = [
        ["$^X -v",                                  qr/larry\s+wall/i, ],
        [[$^X, '-v'],                               qr/larry\s+wall/i, ],
        ["$^X -eprint1 | $^X -neprint",             qr/1/,             ],
        [[$^X,qw[-eprint1 |], $^X, qw|-neprint|],   qr/1/,             ],
    ];

    my @prefs = ( [1,1], [0,1], [0,0] );

    ### if IPC::Run is already loaded,remove tests involving IPC::Run
    ### when on win32
    shift @prefs if $Skip_IPC_Run;

    for my $pref ( @prefs ) {
        $IPC::Cmd::USE_IPC_RUN    = $IPC::Cmd::USE_IPC_RUN      = $pref->[0];
        $IPC::Cmd::USE_IPC_OPEN3  = $IPC::Cmd::USE_IPC_OPEN3    = $pref->[1];

        for my $aref ( @$map ) {
            my $cmd     = $aref->[0];
            my $regex   = $aref->[1];

            my $Can_Buffer;
            my $captured;
            my $ok = run( command => $cmd,
                          buffer  => \$captured,
                    );

            ok($ok,     q[Successful run of command] );

            SKIP: {
                skip "No buffers returned", 1 unless $captured;
                like( $captured, $regex,      q[   Buffer filled] );

                ### if we get here, we have buffers ###
                $Can_Buffer++;
            }

            my @list = run( command => $cmd );
            ok( $list[0],       "Command ran successfully" );
            ok( !$list[1],      "   No error code set" );

            SKIP: {
                skip "No buffers, cannot do buffer tests", 3
                        unless $Can_Buffer;

                ok( (grep /$regex/, @{$list[2]}),
                                    "   Out buffer filled" );
                SKIP: {
                    skip "IPC::Run bug prevents separated " .
                            "stdout/stderr buffers", 2 if $pref->[0];

                    ok( (grep /$regex/, @{$list[3]}),
                                        "   Stdout buffer filled" );
                    ok( @{$list[4]} == 0,
                                        "   Stderr buffer empty" );
                }
            }
        }
    }
}


