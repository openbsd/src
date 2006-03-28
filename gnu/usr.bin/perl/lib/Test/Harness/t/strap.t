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

use Test::More tests => 89;

BEGIN { use_ok('Test::Harness::Straps'); }

my $strap = Test::Harness::Straps->new;
isa_ok( $strap, 'Test::Harness::Straps', 'new()' );

### Testing _is_diagnostic()

my $comment;
ok( !$strap->_is_diagnostic("foo", \$comment), '_is_diagnostic(), not a comment'  );
ok( !defined $comment,                      '  no comment set'              );

ok( !$strap->_is_diagnostic("f # oo", \$comment), '  not a comment with #'     );
ok( !defined $comment,                         '  no comment set'           );

my %comments = (
                "# stuff and things # and stuff"    => 
                                        ' stuff and things # and stuff',
                "    # more things "                => ' more things ',
                "#"                                 => '',
               );

for my $line ( sort keys %comments ) {
    my $line_comment = $comments{$line};
    my $strap = Test::Harness::Straps->new;
    isa_ok( $strap, 'Test::Harness::Straps' );

    my $name = substr($line, 0, 20);
    ok( $strap->_is_diagnostic($line, \$comment),        "  comment '$name'"   );
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
    isa_ok( $strap, 'Test::Harness::Straps' );

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

for my $header ( sort keys %headers ) {
    my $expect = $headers{$header};
    my $strap = Test::Harness::Straps->new;
    isa_ok( $strap, 'Test::Harness::Straps' );

    ok( $strap->_is_header($header),    "_is_header() is a header '$header'" );

    is( $strap->{skip_all}, $expect->{skip_all},      '  skip_all set right' )
      if defined $expect->{skip_all};

    ok( eq_set( [map $strap->{$_},  grep defined $strap->{$_},  @attribs],
                [map $expect->{$_}, grep defined $expect->{$_}, @attribs] ),
        '  the right attributes are there' );
}



### Test _is_bail_out()

my %bails = (
             'Bail out!'                 =>  undef,
             'Bail out!  Wing on fire.'  => 'Wing on fire.',
             'BAIL OUT!'                 => undef,
             'bail out! - Out of coffee' => '- Out of coffee',
            );

for my $line ( sort keys %bails ) {
    my $expect = $bails{$line};
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
