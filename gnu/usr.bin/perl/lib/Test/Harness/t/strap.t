#!/usr/bin/perl -Tw

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;

use Test::More tests => 170;

use_ok('Test::Harness::Straps');

my $strap = Test::Harness::Straps->new;
isa_ok( $strap, 'Test::Harness::Straps', 'new()' );

### Testing _is_comment()

my $comment;
ok( !$strap->_is_comment("foo", \$comment), '_is_comment(), not a comment'  );
ok( !defined $comment,                      '  no comment set'              );

ok( !$strap->_is_comment("f # oo", \$comment), '  not a comment with #'     );
ok( !defined $comment,                         '  no comment set'           );

my %comments = (
                "# stuff and things # and stuff"    => 
                                        ' stuff and things # and stuff',
                "    # more things "                => ' more things ',
                "#"                                 => '',
               );

while( my($line, $line_comment) = each %comments ) {
    my $strap = Test::Harness::Straps->new;
    isa_ok( $strap, 'Test::Harness::Straps' );

    my $name = substr($line, 0, 20);
    ok( $strap->_is_comment($line, \$comment),        "  comment '$name'"   );
    is( $comment, $line_comment,                      '  right comment set' );
}



### Testing _is_header()

my @not_headers = (' 1..2',
                   '1..M',
                   '1..-1',
                   '2..2',
                   '1..a',
                   '',
                  );

foreach my $unheader (@not_headers) {
    my $strap = Test::Harness::Straps->new;

    ok( !$strap->_is_header($unheader),     
        "_is_header(), not a header '$unheader'" );

    ok( (!grep { exists $strap->{$_} } qw(max todo skip_all)),
        "  max, todo and skip_all are not set" );
}


my @attribs = qw(max skip_all todo);
my %headers = (
   '1..2'                               => { max => 2 },
   '1..1'                               => { max => 1 },
   '1..0'                               => { max => 0,
                                             skip_all => '',
                                           },
   '1..0 # Skipped: no leverage found'  => { max      => 0,
                                             skip_all => 'no leverage found',
                                           },
   '1..4 # Skipped: no leverage found'  => { max      => 4,
                                             skip_all => 'no leverage found',
                                           },
   '1..0 # skip skip skip because'      => { max      => 0,
                                             skip_all => 'skip skip because',
                                           },
   '1..10 todo 2 4 10'                  => { max        => 10,
                                             'todo'       => { 2  => 1,
                                                               4  => 1,
                                                               10 => 1,
                                                           },
                                           },
   '1..10 todo'                         => { max        => 10 },
   '1..192 todo 4 2 13 192 # Skip skip skip because'   => 
                                           { max     => 192,
                                             'todo'    => { 4   => 1, 
                                                            2   => 1, 
                                                            13  => 1, 
                                                            192 => 1,
                                                        },
                                             skip_all => 'skip skip because'
                                           }
);

while( my($header, $expect) = each %headers ) {
    my $strap = Test::Harness::Straps->new;
    isa_ok( $strap, 'Test::Harness::Straps' );

    ok( $strap->_is_header($header),    "_is_header() is a header '$header'" );

    is( $strap->{skip_all}, $expect->{skip_all},      '  skip_all set right' )
      if defined $expect->{skip_all};

    ok( eq_set( [map $strap->{$_},  grep defined $strap->{$_},  @attribs],
                [map $expect->{$_}, grep defined $expect->{$_}, @attribs] ),
        '  the right attributes are there' );
}



### Testing _is_test()

my %tests = (
             'ok'       => { 'ok' => 1 },
             'not ok'   => { 'ok' => 0 },

             'ok 1'     => { 'ok' => 1, number => 1 },
             'not ok 1' => { 'ok' => 0, number => 1 },

             'ok 2938'  => { 'ok' => 1, number => 2938 },

             'ok 1066 - and all that'   => { 'ok'     => 1,
                                             number => 1066,
                                             name   => "- and all that" },
             'not ok 42 - universal constant'   => 
                                      { 'ok'     => 0,
                                        number => 42,
                                        name   => '- universal constant',
                                      },
             'not ok 23 # TODO world peace'     => { 'ok'     => 0,
                                                     number => 23,
                                                     type   => 'todo',
                                                     reason => 'world peace'
                                                   },
             'ok 11 - have life # TODO get a life'  => 
                                      { 'ok'     => 1,
                                        number => 11,
                                        name   => '- have life',
                                        type   => 'todo',
                                        reason => 'get a life'
                                      },
             'not ok # TODO'    => { 'ok'     => 0,
                                     type   => 'todo',
                                     reason => ''
                                   },
             'ok # skip'        => { 'ok'     => 1,
                                     type   => 'skip',
                                   },
             'not ok 11 - this is \# all the name # skip this is not'
                                => { 'ok'     => 0,
                                     number => 11,
                                     name   => '- this is \# all the name',
                                     type   => 'skip',
                                     reason => 'this is not'
                                   },
             "ok 42 - _is_header() is a header '1..192 todo 4 2 13 192 \\# Skip skip skip because"
                                => { 'ok'   => 1,
                                     number => 42,
                                     name   => "- _is_header() is a header '1..192 todo 4 2 13 192 \\# Skip skip skip because",
                                   },
            );

while( my($line, $expect) = each %tests ) {
    my %test;
    ok( $strap->_is_test($line, \%test),    "_is_test() spots '$line'" );

    foreach my $type (qw(ok number name type reason)) {
        cmp_ok( $test{$type}, 'eq', $expect->{$type}, "  $type" );
    }
}

my @untests = (
               ' ok',
               'not',
               'okay 23',
              );
foreach my $line (@untests) {
    my $strap = Test::Harness::Straps->new;
    isa_ok( $strap, 'Test::Harness::Straps' );

    my %test = ();
    ok( !$strap->_is_test($line, \%test),    "_is_test() disregards '$line'" );

    # is( keys %test, 0 ) won't work in 5.004 because it's undef.
    ok( !keys %test,                         '  and produces no test info'   );
}


### Test _is_bail_out()

my %bails = (
             'Bail out!'                 =>  undef,
             'Bail out!  Wing on fire.'  => 'Wing on fire.',
             'BAIL OUT!'                 => undef,
             'bail out! - Out of coffee' => '- Out of coffee',
            );

while( my($line, $expect) = each %bails ) {
    my $strap = Test::Harness::Straps->new;
    isa_ok( $strap, 'Test::Harness::Straps' );

    my $reason;
    ok( $strap->_is_bail_out($line, \$reason), "_is_bail_out() spots '$line'");
    is( $reason, $expect,                       '  with the right reason' );
}

my @unbails = (
               '  Bail out!',
               'BAIL OUT',
               'frobnitz',
               'ok 23 - BAIL OUT!',
              );

foreach my $line (@unbails) {
    my $strap = Test::Harness::Straps->new;
    isa_ok( $strap, 'Test::Harness::Straps' );

    my $reason;

    ok( !$strap->_is_bail_out($line, \$reason),  
                                       "_is_bail_out() ignores '$line'" );
    is( $reason, undef,                         '  and gives no reason' );
}
