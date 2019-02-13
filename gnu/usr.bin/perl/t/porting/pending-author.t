#!./perl -w

# What does this test?
# This uses Porting/checkAUTHORS.pl to check that any pending commit isn't
# about to break t/porting/authors.t
#
# Why do we test this?
# t/porting/authors.t checks that the AUTHORS file is up to date, accounting
# for the "Author:" of every commit. However, any pending changes can't be
# tested, which leaves a gotcha - "make test" can pass, one then commits
# the passing code, pushes it uptream, and tests fail. So this test attempts
# to spot that problem before it happens, where it can.
#
# It's broken - how do I fix it?
# It will fail if you're in a git checkout, have uncommitted changes, and the
# e-mail address that your commit will default to is in AUTHORS, or the list
# of author aliases in Porting/checkAUTHORS.pl. So one of
# a) reset your pending changes
# b) change your git config user.email to the previously-known e-mail address
# c) add yourself to AUTHORS
# d) add an alias to Porting/checkAUTHORS.pl

BEGIN {
    @INC = '..' if -f '../TestInit.pm';
}
use TestInit qw(T); # T is chdir to the top level
use strict;
use File::Spec;

require './t/test.pl';
find_git_or_skip('all');

my $devnull = File::Spec->devnull;
my @lines = `git status --porcelain 2>$devnull`;
skip_all("git status --porcelain doesn't seem to work here")
    if $? != 0;
skip_all("No pending changes")
    if !grep !/^\?\?/, @lines;

sub get {
    my $key = shift;
    my $value = `git config --get user.$key`;
    unless (defined $value && $value =~ /\S/) {
	skip_all("git config --get user.$key returned nought");
    }
    chomp $value;
    return $value;
}

my $email = get('email');
my $name = get('name');

open my $fh, '|-', "$^X Porting/checkAUTHORS.pl --tap -"
    or die $!;
print $fh "Author: $name <$email>\n";
close $fh or die $!;
