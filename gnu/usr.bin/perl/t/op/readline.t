#!./perl

BEGIN {
    chdir 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 13;

eval { for (\2) { $_ = <FH> } };
like($@, 'Modification of a read-only value attempted', '[perl #19566]');

{
  open A,"+>a"; $a = 3;
  is($a .= <A>, 3, '#21628 - $a .= <A> , A eof');
  close A; $a = 4;
  is($a .= <A>, 4, '#21628 - $a .= <A> , A closed');
  unlink "a";
}

# 82 is chosen to exceed the length for sv_grow in do_readline (80)
foreach my $k (1, 82) {
  my $result
    = runperl (stdin => '', stderr => 1,
              prog => "\$x = q(k) x $k; \$a{\$x} = qw(v); \$_ = <> foreach keys %a; print qw(end)",
	      );
  $result =~ s/\n\z// if $^O eq 'VMS';
  is ($result, "end", '[perl #21614] for length ' . length('k' x $k));
}


foreach my $k (1, 21) {
  my $result
    = runperl (stdin => ' rules', stderr => 1,
              prog => "\$x = q(perl) x $k; \$a{\$x} = q(v); foreach (keys %a) {\$_ .= <>; print}",
	      );
  $result =~ s/\n\z// if $^O eq 'VMS';
  is ($result, ('perl' x $k) . " rules", 'rcatline to shared sv for length ' . length('perl' x $k));
}

# These COW tests are not going to show up anything on 5.8.x (No Copy On Write)
# but they do no harm, and it makes life easier to keep this file fully in
# sync with 5.9.x

foreach my $l (1, 82) {
  my $k = $l;
  $k = 'k' x $k;
  my $copy = $k;
  $k = <DATA>;
  is ($k, "moo\n", 'catline to COW sv for length ' . length $copy);
}


foreach my $l (1, 21) {
  my $k = $l;
  $k = 'perl' x $k;
  my $perl = $k;
  $k .= <DATA>;
  is ($k, "$perl rules\n", 'rcatline to COW sv for length ' . length $perl);
}

use strict;
use File::Spec;

open F, File::Spec->curdir and sysread F, $_, 1;
my $err = $! + 0;
close F;

SKIP: {
  skip "you can read directories as plain files", 2 unless( $err );

  $!=0;
  open F, File::Spec->curdir and $_=<F>;
  ok( $!==$err && !defined($_) => 'readline( DIRECTORY )' );
  close F;

  $!=0;
  { local $/;
    open F, File::Spec->curdir and $_=<F>;
    ok( $!==$err && !defined($_) => 'readline( DIRECTORY ) slurp mode' );
    close F;
  }
}

__DATA__
moo
moo
 rules
 rules
