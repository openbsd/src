#!/usr/pkg/bin/perl
#
# Sample cracklib password verifier for Heimdals external password
# verifier, see the chapter "Password changing" in the the info
# documentation for more information about the protocol used.
#
# $KTH: check-cracklib.pl,v 1.1 2005/04/15 12:29:51 lha Exp $

use strict;
use Crypt::Cracklib;

my $database = '/usr/pkg/libdata/pw_dict';

my %params;

while (<>) {
    last if /^end$/;
    if (!/^([^:]+): (.+)$/) {
	die "key value pair not correct: $_";
    }
    $params{$1} = $2;
}

die "missing password" if (!defined $params{'new-password'});

my $reason = fascist_check($params{'new-password'}, $database);

if ($reason eq "ok") {
    print "APPROVED\n";
} else {
    print "$reason\n";
}

exit 0

