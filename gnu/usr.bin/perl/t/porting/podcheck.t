#!/usr/bin/perl -w

require './test.pl';

use strict;

{
    package My::Pod::Checker;
    use strict;
    use parent 'Pod::Checker';

    use vars '@errors'; # a bad, bad hack!

    sub poderror {
        my $self = shift;
        my $opts;
        if (ref $_[0]) {
            $opts = shift;
        };
        ++($self->{_NUM_ERRORS})
            if(!$opts || ($opts->{-severity} && $opts->{-severity} eq 'ERROR'));
        ++($self->{_NUM_WARNINGS})
            if(!$opts || ($opts->{-severity} && $opts->{-severity} eq 'WARNING'));
        push @errors, $opts;
    };
}


use strict;
use File::Spec;
s{^\.\./lib$}{lib} for @INC;
chdir '..';
my @files;
my $manifest = 'MANIFEST';

open my $m, '<', $manifest or die "Can't open '$manifest': $!";

while (<$m>) {
    chomp;
    next unless /\s/;   # Ignore lines without whitespace (i.e., filename only)
    my ($file, $separator) = /^(\S+)(\s+)/;
	next if $file =~ /^cpan\//;
	next unless ($file =~ /\.(?:pm|pod|pl)$/);
    push @files, $file;
};
@files = sort @files; # so we get consistent results

sub pod_ok {
    my ($filename) = @_;
    local @My::Pod::Checker::errors;
    my $checker = My::Pod::Checker->new(-quiet => 1);
    $checker->parse_from_file($filename, undef);
    my $error_count = $checker->num_errors();

    if(! ok($error_count <= 0, "POD of $filename")) {
        diag( "'$filename' contains POD errors" );
        diag(sprintf "%s %s: %s at line %s",
             $_->{-severity}, $_->{-file}, $_->{-msg}, $_->{-line})
            for @My::Pod::Checker::errors;
    };
};

plan (tests => scalar @files);

pod_ok $_
    for @files;

__DATA__
lib/
ext/
pod/
AUTHORS
Changes
INSTALL
README*
*.pod
