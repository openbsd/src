#!perl -w

use Test::More tests => 19;

use XS::APItest;

for my $func ('SvPVbyte', 'SvPVutf8') {
 $g = *glob;
 $r = \1;
 is &$func($g), '*main::glob', "$func(\$glob_copy)";
 is ref\$g, 'GLOB', "$func(\$glob_copy) does not flatten the glob";
 is &$func($r), "$r", "$func(\$ref)";
 is ref\$r, 'REF', "$func(\$ref) does not flatten the ref";

 is &$func(*glob), '*main::glob', "$func(*glob)";
 is ref\$::{glob}, 'GLOB', "$func(*glob) does not flatten the glob";
 is &$func($^V), "$^V", "$func(\$ro_ref)";
 is ref\$^V, 'REF', "$func(\$ro_ref) does not flatten the ref";
}

eval 'SvPVbyte(*{chr 256})';
like $@, qr/^Wide character/, 'SvPVbyte fails on Unicode glob';
package r { use overload '""' => sub { substr "\x{100}\xff", -1 } }
is SvPVbyte(bless [], r::), "\xff",
  'SvPVbyte on ref returning downgradable utf8 string';

sub TIESCALAR { bless \(my $thing = pop), shift }
sub FETCH { ${ +shift } }
tie $tyre, main => bless [], r::;
is SvPVbyte($tyre), "\xff",
  'SvPVbyte on tie returning ref that returns downgradable utf8 string';
