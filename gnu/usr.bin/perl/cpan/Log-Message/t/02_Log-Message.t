### Log::Message test suite ###
BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir '../lib/Log/Message' if -d '../lib/Log/Message';
        unshift @INC, '../../..';
    }
}

BEGIN { chdir 't' if -d 't' }


use strict;
use lib qw[../lib to_load];
use Test::More tests => 34;

### use tests
for my $pkg ( qw[ Log::Message          Log::Message::Config
                  Log::Message::Item    Log::Message::Handlers]
) {
    use_ok( $pkg ) or diag "'$pkg' not found. Dying";
}

### test global stack
{
    my $log = Log::Message->new( private => 0 );
    is( $log->{STACK}, $Log::Message::STACK, q[Using global stack] );
}

### test using private stack
{
    my $log = Log::Message->new( private => 1 );
    isnt( $log->{STACK}, $Log::Message::STACK, q[Using private stack] );

    $log->store('foo'); $log->store('bar');

    ### retrieval tests
    {
        my @list = $log->retrieve();

        ok( @list == 2, q[Stored 2 messages] );
    }

    $log->store('zot'); $log->store('quux');

    {
        my @list = $log->retrieve( amount => 3 );

        ok( @list == 3, q[Retrieving 3 messages] );
    }

    {
        is( $log->first->message, 'foo',    q[  Retrieving first message] );
        is( $log->final->message, 'quux',   q[  Retrieving final message] );
    }

    {
        package Log::Message::Handlers;

        sub test    { return shift }
        sub test2   { shift; return @_ }

        package main;
    }

    $log->store(
            message     => 'baz',
            tag         => 'MY TAG',
            level       => 'test',
    );

    {
        ok( $log->retrieve( message => qr/baz/ ),
                                        q[  Retrieving based on message] );
        ok( $log->retrieve( tag     => qr/TAG/ ),
                                        q[  Retrieving based on tag] );
        ok( $log->retrieve( level   => qr/test/ ),
                                        q[  Retrieving based on level] );
    }

    my $item = $log->retrieve( chrono => 0 );

    {
        ok( $item,                      q[Retrieving item] );
        is( $item->parent,  $log,       q[  Item reference to parent] );
        is( $item->message, 'baz',      q[  Item message stored] );
        is( $item->id,      4,          q[  Item id stored] );
        is( $item->tag,     'MY TAG',   q[  Item tag stored] );
        is( $item->level,   'test',     q[  Item level stored] );
    }

    {
        ### shortmess is very different from 5.6.1 => 5.8, so let's
        ### just check that it is filled.
        ok(     $item->shortmess,       q[Item shortmess stored] );
        like(   $item->shortmess, qr/\w+/,
                q[  Item shortmess stored properly]
        );

        ok(     $item->longmess,        q[Item longmess stored] );
        like(   $item->longmess, qr/Log::Message::store/s,
                q[  Item longmess stored properly]
        );

        my $t = scalar localtime;
        $t =~ /(\w+ \w+ \d+)/;

        like(   $item->when, qr/$1/, q[Item timestamp stored] );
    }

    {
        my $i = $item->test;
        my @a = $item->test2(1,2,3);

        is( $item, $i,              q[Item handler check] );
        is_deeply( $item, $i,       q[  Item handler deep check] );
        is_deeply( \@a, [1,2,3],    q[  Item extra argument check] );
    }

    {
        ok( $item->remove,          q[Removing item from stack] );
        ok( (!grep{ $item eq $_ } $log->retrieve),
                                    q[  Item removed from stack] );
    }

    {
        $log->flush;
        ok( @{$log->{STACK}} == 0,  q[Flushing stack] );
    }
}

### test errors
{   my $log = Log::Message->new( private => 1 );


    ### store errors
    {   ### dont make it print
        my $warnings;
        local $SIG{__WARN__} = sub { $warnings .= "@_" };

        my $rv  = $log->store();
        ok( !$rv,                       q[Logging empty message failed] );
        like( $warnings, qr/message/,   q[  Spotted the error] );
    }

    ### retrieve errors
    {   ### dont make it print
        my $warnings;
        local $SIG{__WARN__} = sub { $warnings .= "@_" };

        ### XXX whitebox test!
        local $Params::Check::VERBOSE = 1; # so the warnings are emitted
        local $Params::Check::VERBOSE = 1; # so the warnings are emitted

        my $rv  = $log->retrieve( frobnitz => $$ );
        ok( !$rv,                       q[Retrieval with bogus args] );
        like( $warnings, qr/not a valid key/,
                                        qq[  Spotted the error] );
    }
}

















