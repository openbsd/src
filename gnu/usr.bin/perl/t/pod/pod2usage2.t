#!/usr/bin/perl -w

use Test::More;

BEGIN {
  if ($^O eq 'MSWin32' || $^O eq 'VMS') {
    plan skip_all => "Not portable on Win32 or VMS\n";
  }
  else {
    plan tests => 15;
  }
  use_ok ("Pod::Usage");
}

sub getoutput
{
  my ($code) = @_;
  my $pid = open(IN, "-|");
  unless(defined $pid) {
    die "Cannot fork: $!";
  }
  if($pid) {
    # parent
    my @out = <IN>;
    close(IN);
    my $exit = $?>>8;
    s/^/#/ for @out;
    local $" = "";
    print "#EXIT=$exit OUTPUT=+++#@out#+++\n";
    return($exit, join("",@out));
  }
  # child
  open(STDERR, ">&STDOUT");
  &$code;
  print "--NORMAL-RETURN--\n";
  exit 0;
}

sub compare
{
  my ($left,$right) = @_;
  $left  =~ s/^#\s+/#/gm;
  $right =~ s/^#\s+/#/gm;
  $left  =~ s/\s+/ /gm;
  $right =~ s/\s+/ /gm;
  $left eq $right;
}

my ($exit, $text) = getoutput( sub { pod2usage() } );
is ($exit, 2,                 "Exit status pod2usage ()");
ok (compare ($text, <<'EOT'), "Output test pod2usage ()");
#Usage:
#    frobnicate [ -r | --recursive ] [ -f | --force ] [ -n number ] file ...
#
EOT

($exit, $text) = getoutput( sub { pod2usage(
  -message => 'You naughty person, what did you say?',
  -verbose => 1 ) });
is ($exit, 1,                 "Exit status pod2usage (-message => '...', -verbose => 1)");
ok (compare ($text, <<'EOT'), "Output test pod2usage (-message => '...', -verbose => 1)");
#You naughty person, what did you say?
# Usage:
#     frobnicate [ -r | --recursive ] [ -f | --force ] [ -n number ] file ...
# 
# Options:
#     -r | --recursive
#         Run recursively.
# 
#     -f | --force
#         Just do it!
# 
#     -n number
#         Specify number of frobs, default is 42.
# 
EOT

($exit, $text) = getoutput( sub { pod2usage(
  -verbose => 2, -exit => 42 ) } );
is ($exit, 42,                "Exit status pod2usage (-verbose => 2, -exit => 42)");
ok (compare ($text, <<'EOT'), "Output test pod2usage (-verbose => 2, -exit => 42)");
#NAME
#     frobnicate - do what I mean
#
# SYNOPSIS
#     frobnicate [ -r | --recursive ] [ -f | --force ] [ -n number ] file ...
#
# DESCRIPTION
#     frobnicate does foo and bar and what not.
#
# OPTIONS
#     -r | --recursive
#         Run recursively.
#
#     -f | --force
#         Just do it!
#
#     -n number
#         Specify number of frobs, default is 42.
#
EOT

($exit, $text) = getoutput( sub { pod2usage(0) } );
is ($exit, 0,                 "Exit status pod2usage (0)");
ok (compare ($text, <<'EOT'), "Output test pod2usage (0)");
#Usage:
#     frobnicate [ -r | --recursive ] [ -f | --force ] [ -n number ] file ...
#
# Options:
#     -r | --recursive
#         Run recursively.
#
#     -f | --force
#         Just do it!
#
#     -n number
#         Specify number of frobs, default is 42.
#
EOT

($exit, $text) = getoutput( sub { pod2usage(42) } );
is ($exit, 42,                "Exit status pod2usage (42)");
ok (compare ($text, <<'EOT'), "Output test pod2usage (42)");
#Usage:
#     frobnicate [ -r | --recursive ] [ -f | --force ] [ -n number ] file ...
#
EOT

($exit, $text) = getoutput( sub { pod2usage(-verbose => 0, -exit => 'NOEXIT') } );
is ($exit, 0,                 "Exit status pod2usage (-verbose => 0, -exit => 'NOEXIT')");
ok (compare ($text, <<'EOT'), "Output test pod2usage (-verbose => 0, -exit => 'NOEXIT')");
#Usage:
#     frobnicate [ -r | --recursive ] [ -f | --force ] [ -n number ] file ...
#
# --NORMAL-RETURN--
EOT

($exit, $text) = getoutput( sub { pod2usage(-verbose => 99, -sections => 'DESCRIPTION') } );
is ($exit, 1,                 "Exit status pod2usage (-verbose => 99, -sections => 'DESCRIPTION')");
ok (compare ($text, <<'EOT'), "Output test pod2usage (-verbose => 99, -sections => 'DESCRIPTION')");
#Description:
#     frobnicate does foo and bar and what not.
#
EOT



__END__

=head1 NAME

frobnicate - do what I mean

=head1 SYNOPSIS

B<frobnicate> S<[ B<-r> | B<--recursive> ]> S<[ B<-f> | B<--force> ]>
  S<[ B<-n> I<number> ]> I<file> ...

=head1 DESCRIPTION

B<frobnicate> does foo and bar and what not.

=head1 OPTIONS

=over 4

=item B<-r> | B<--recursive>

Run recursively.

=item B<-f> | B<--force>

Just do it!

=item B<-n> I<number>

Specify number of frobs, default is 42.

=back

=cut

