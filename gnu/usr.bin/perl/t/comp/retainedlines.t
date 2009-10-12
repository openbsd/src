#!./perl -w

# Check that lines from eval are correctly retained by the debugger

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require "./test.pl";
}

use strict;

plan (tests => 57);

$^P = 0xA;

my @before = grep { /eval/ } keys %::;

is (@before, 0, "No evals");

my %seen;

sub check_retained_lines {
    my ($prog, $name) = @_;
    # Is there a more efficient way to write this?
    my @expect_lines = (undef, map ({"$_\n"} split "\n", $prog), "\n", ';');

    my @keys = grep {!$seen{$_}} grep { /eval/ } keys %::;

    is (@keys, 1, "1 new eval");

    my @got_lines = @{$::{$keys[0]}};

    is (@got_lines, @expect_lines, "Right number of lines for $name");

    for (0..$#expect_lines) {
	is ($got_lines[$_], $expect_lines[$_], "Line $_ is correct");
    }
    $seen{$keys[0]}++;
}

my $name = 'foo';

for my $sep (' ', "\0") {

    my $prog = "sub $name {
    'Perl${sep}Rules'
};
1;
";

    eval $prog or die;
    check_retained_lines($prog, ord $sep);
    $name++;
}

{
  # This contains a syntax error
  my $prog = "sub $name {
    'This is $name'
  }
1 +
";

  eval $prog and die;

  is (eval "$name()", "This is $name", "Subroutine was compiled, despite error")
    or diag $@;

  my @after = grep { /eval/ } keys %::;

  is (@after, 0 + keys %seen,
      "current behaviour is that errors in eval trump subroutine definitions");

  $name++;
}

foreach my $flags (0x0, 0x800, 0x1000, 0x1800) {
    local $^P = $^P | $flags;
    # This is easier if we accept that the guts eval will add a trailing \n
    # for us
    my $prog = "1 + 1 + 1\n";
    my $fail = "1 + \n";

    is (eval $prog, 3, 'String eval works');
    if ($flags & 0x800) {
	check_retained_lines($prog, sprintf "%#X", $^P);
    } else {
	my @after = grep { /eval/ } keys %::;

	is (@after, 0 + keys %seen,
	    "evals that don't define subroutines are correctly cleaned up");
    }

    is (eval $fail, undef, 'Failed string eval fails');

    if ($flags & 0x1000) {
    TODO: {
	    todo_skip "Can't yet retain lines for evals with syntax errors", 6;
	    check_retained_lines($fail, sprintf "%#X", $^P);
	}
    } else {
	my @after = grep { /eval/ } keys %::;

	is (@after, 0 + keys %seen,
	    "evals that fail are correctly cleaned up");
    }
}
