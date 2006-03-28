#!/usr/bin/perl -w

# Can't use Test.pm, that's a 5.005 thing.
package My::Test;

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

unless( eval { require File::Spec } ) {
    print "1..0 # Skip Need File::Spec to run this test\n";
    exit 0;
}

if( $^O eq 'VMS' && $] <= 5.00503 ) {
    print "1..0 # Skip test will hang on older VMS perls\n";
    exit 0;
}

if( $^O eq 'MacOS' ) {
    print "1..0 # Skip exit status broken on Mac OS\n";
    exit 0;
}

my $test_num = 1;
# Utility testing functions.
sub ok ($;$) {
    my($test, $name) = @_;
    my $ok = '';
    $ok .= "not " unless $test;
    $ok .= "ok $test_num";
    $ok .= " - $name" if defined $name;
    $ok .= "\n";
    print $ok;
    $test_num++;
}


package main;

my $IsVMS = $^O eq 'VMS';

print "# Ahh!  I see you're running VMS.\n" if $IsVMS;

my %Tests = (
             #                      Everyone Else   VMS
             'success.plx'              => [0,      0],
             'one_fail.plx'             => [1,      4],
             'two_fail.plx'             => [2,      4],
             'five_fail.plx'            => [5,      4],
             'extras.plx'               => [2,      4],
             'too_few.plx'              => [255,    4],
             'too_few_fail.plx'         => [2,      4],
             'death.plx'                => [255,    4],
             'last_minute_death.plx'    => [255,    4],
             'pre_plan_death.plx'       => ['not zero',    'not zero'],
             'death_in_eval.plx'        => [0,      0],
             'require.plx'              => [0,      0],
	     'exit.plx'                 => [1,      4],
            );

print "1..".keys(%Tests)."\n";

eval { require POSIX; &POSIX::WEXITSTATUS(0) };
if( $@ ) {
    *exitstatus = sub { $_[0] >> 8 };
}
else {
    *exitstatus = sub { POSIX::WEXITSTATUS($_[0]) }
}

chdir 't';
my $lib = File::Spec->catdir(qw(lib Test Simple sample_tests));
while( my($test_name, $exit_codes) = each %Tests ) {
    my($exit_code) = $exit_codes->[$IsVMS ? 1 : 0];

    my $Perl = $^X;

    if( $^O eq 'VMS' ) {
        # VMS can't use its own $^X in a system call until almost 5.8
        $Perl = "MCR $^X" if $] < 5.007003;

        # Quiet noisy 'SYS$ABORT'.  'hushed' only exists in 5.6 and up,
        # but it doesn't do any harm on eariler perls.
        $Perl .= q{ -"Mvmsish=hushed"};
    }

    my $file = File::Spec->catfile($lib, $test_name);
    my $wait_stat = system(qq{$Perl -"I../blib/lib" -"I../lib" -"I../t/lib" $file});
    my $actual_exit = exitstatus($wait_stat);

    if( $exit_code eq 'not zero' ) {
        My::Test::ok( $actual_exit != 0,
                      "$test_name exited with $actual_exit ".
                      "(expected $exit_code)");
    }
    else {
        My::Test::ok( $actual_exit == $exit_code, 
                      "$test_name exited with $actual_exit ".
                      "(expected $exit_code)");
    }
}
