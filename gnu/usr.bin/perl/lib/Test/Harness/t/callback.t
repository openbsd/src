#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use Test::More;
use File::Spec;

my $Curdir = File::Spec->curdir;
my $SAMPLE_TESTS = $ENV{PERL_CORE}
                    ? File::Spec->catdir($Curdir, 'lib', 'sample-tests')
                    : File::Spec->catdir($Curdir, 't',   'sample-tests');

%samples = (
            bailout     => [qw( header test test test bailout )],
            combined    => ['header', ('test') x 10],
            descriptive => ['header', ('test') x 5 ],
            duplicates  => ['header', ('test') x 11 ],
            head_end    => [qw( other test test test test 
                                other header other other )],
            head_fail   => [qw( other test test test test
                                other header other other )],
            no_nums     => ['header', ('test') x 5 ],
            out_of_order=> [('test') x 10, 'header', ('test') x 5],
            simple      => [qw( header test test test test test )],
            simple_fail => [qw( header test test test test test )],
            'skip'      => [qw( header test test test test test )],
            skipall     => [qw( header )],
            skipall_nomsg => [qw( header )],
            skip_nomsg  => [qw( header test )],
            taint       => [qw( header test )],
            'todo'      => [qw( header test test test test test )],
            todo_inline => [qw( header test test test )],
            vms_nit     => [qw( header other test test )],
            with_comments => [qw( other header other test other test test
                                  test other other test other )],
           );

plan tests => 2 + scalar keys %samples;

use_ok( 'Test::Harness::Straps' );
my $strap = Test::Harness::Straps->new;
isa_ok( $strap, 'Test::Harness::Straps' );
$strap->{callback} = sub {
    my($self, $line, $type, $totals) = @_;
    push @out, $type;
};

while( my($test, $expect) = each %samples ) {
    local @out = ();
    $strap->analyze_file(File::Spec->catfile($SAMPLE_TESTS, $test));

    is_deeply(\@out, $expect,   "$test callback");
}
