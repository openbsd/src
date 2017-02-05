use 5.008001;

use strict;
use warnings;

my $number = 0;
sub ok {
        my ($condition, $name) = @_;

        my $message = $condition ? "ok " : "not ok ";
        $message .= ++$number;
        $message .= " # $name" if defined $name;
        print $message, "\n";
        return $condition;
}

sub is {
        my ($got, $expected, $name) = @_;

        for ($got, $expected) {
                $_ = 'undef' unless defined $_;
        }

        unless (ok($got eq $expected, $name)) {
                warn "Got: '$got'\nExpected: '$expected'\n" . join(' ', caller) . "\n";
        }
}

sub skip {
        my ($reason, $num) = @_;
        $reason ||= '';
        $number ||= 1;

        for (1 .. $num) {
                $number++;
                print "ok $number # skip $reason\n";
        }
}

1;

