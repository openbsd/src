#!/usr/bin/perl
# 
# Regenerate (overwriting only if changed):
#
#    lib/feature.pm
#    feature.h
#
# from information hardcoded into this script and from two #defines
# in perl.h.
#
# This script is normally invoked from regen.pl.

BEGIN {
    require 'regen/regen_lib.pl';
    push @INC, './lib';
}
use strict ;


###########################################################################
# Hand-editable data

# (feature name) => (internal name, used in %^H and macro names)
my %feature = (
    say             => 'say',
    state           => 'state',
    switch          => 'switch',
    evalbytes       => 'evalbytes',
    postderef       => 'postderef',
    array_base      => 'arybase',
    current_sub     => '__SUB__',
    lexical_subs    => 'lexsubs',
    postderef_qq    => 'postderef_qq',
    unicode_eval    => 'unieval',
    unicode_strings => 'unicode',
    fc              => 'fc',
    signatures      => 'signatures',
);

# NOTE: If a feature is ever enabled in a non-contiguous range of Perl
#       versions, any code below that uses %BundleRanges will have to
#       be changed to account.

# 5.odd implies the next 5.even, but an explicit 5.even can override it.
my %feature_bundle = (
     all     => [ keys %feature ],
     default =>	[qw(array_base)],
    "5.9.5"  =>	[qw(say state switch array_base)],
    "5.10"   =>	[qw(say state switch array_base)],
    "5.11"   =>	[qw(say state switch unicode_strings array_base)],
    "5.13"   =>	[qw(say state switch unicode_strings array_base)],
    "5.15"   =>	[qw(say state switch unicode_strings unicode_eval
		    evalbytes current_sub fc)],
    "5.17"   =>	[qw(say state switch unicode_strings unicode_eval
		    evalbytes current_sub fc)],
    "5.19"   =>	[qw(say state switch unicode_strings unicode_eval
		    evalbytes current_sub fc)],
);

# not actually used currently
my @experimental = qw( lexical_subs );


###########################################################################
# More data generated from the above

for (keys %feature_bundle) {
    next unless /^5\.(\d*[13579])\z/;
    $feature_bundle{"5.".($1+1)} ||= $feature_bundle{$_};
}

my %UniqueBundles; # "say state switch" => 5.10
my %Aliases;       #  5.12 => 5.11
for( sort keys %feature_bundle ) {
    my $value = join(' ', sort @{$feature_bundle{$_}});
    if (exists $UniqueBundles{$value}) {
	$Aliases{$_} = $UniqueBundles{$value};
    }
    else {
	$UniqueBundles{$value} = $_;
    }
}
			   # start   end
my %BundleRanges; # say => ['5.10', '5.15'] # unique bundles for values
for my $bund (
    sort { $a eq 'default' ? -1 : $b eq 'default' ? 1 : $a cmp $b }
         values %UniqueBundles
) {
    next if $bund =~ /[^\d.]/ and $bund ne 'default';
    for (@{$feature_bundle{$bund}}) {
	if (@{$BundleRanges{$_} ||= []} == 2) {
	    $BundleRanges{$_}[1] = $bund
	}
	else {
	    push @{$BundleRanges{$_}}, $bund;
	}
    }
}

my $HintShift;
my $HintMask;
my $Uni8Bit;

open "perl.h", "perl.h" or die "$0 cannot open perl.h: $!";
while (readline "perl.h") {
    next unless /#\s*define\s+(HINT_FEATURE_MASK|HINT_UNI_8_BIT)/;
    my $is_u8b = $1 =~ 8;
    /(0x[A-Fa-f0-9]+)/ or die "No hex number in:\n\n$_\n ";
    if ($is_u8b) {
	$Uni8Bit = $1;
    }
    else {
	my $hex = $HintMask = $1;
	my $bits = sprintf "%b", oct $1;
	$bits =~ /^0*1+(0*)\z/
	 or die "Non-contiguous bits in $bits (binary for $hex):\n\n$_\n ";
	$HintShift = length $1;
	my $bits_needed =
	    length sprintf "%b", scalar keys %UniqueBundles;
	$bits =~ /1{$bits_needed}/
	    or die "Not enough bits (need $bits_needed)"
		 . " in $bits (binary for $hex):\n\n$_\n ";
    }
    if ($Uni8Bit && $HintMask) { last }
}
die "No HINT_FEATURE_MASK defined in perl.h" unless $HintMask;
die "No HINT_UNI_8_BIT defined in perl.h"    unless $Uni8Bit;

close "perl.h";

my @HintedBundles =
    ('default', grep !/[^\d.]/, sort values %UniqueBundles);


###########################################################################
# Open files to be generated

my ($pm, $h) = map {
    open_new($_, '>', { by => 'regen/feature.pl' });
} 'lib/feature.pm', 'feature.h';


###########################################################################
# Generate lib/feature.pm

while (<DATA>) {
    last if /^FEATURES$/ ;
    print $pm $_ ;
}

sub longest {
    my $long;
    for(@_) {
	if (!defined $long or length $long < length) {
	    $long = $_;
	}
    }
    $long;
}

print $pm "our %feature = (\n";
my $width = length longest keys %feature;
for(sort { length $a <=> length $b || $a cmp $b } keys %feature) {
    print $pm "    $_" . " "x($width-length)
	    . " => 'feature_$feature{$_}',\n";
}
print $pm ");\n\n";

print $pm "our %feature_bundle = (\n";
$width = length longest values %UniqueBundles;
for( sort { $UniqueBundles{$a} cmp $UniqueBundles{$b} }
          keys %UniqueBundles ) {
    my $bund = $UniqueBundles{$_};
    print $pm qq'    "$bund"' . " "x($width-length $bund)
	    . qq' => [qw($_)],\n';
}
print $pm ");\n\n";

for (sort keys %Aliases) {
    print $pm
	qq'\$feature_bundle{"$_"} = \$feature_bundle{"$Aliases{$_}"};\n';
};

#print $pm "my \%experimental = (\n";
#print $pm "    $_ => 1,\n", for @experimental;
#print $pm ");\n";

print $pm <<EOPM;

our \$hint_shift   = $HintShift;
our \$hint_mask    = $HintMask;
our \@hint_bundles = qw( @HintedBundles );

# This gets set (for now) in \$^H as well as in %^H,
# for runtime speed of the uc/lc/ucfirst/lcfirst functions.
# See HINT_UNI_8_BIT in perl.h.
our \$hint_uni8bit = $Uni8Bit;
EOPM


while (<DATA>) {
    last if /^PODTURES$/ ;
    print $pm $_ ;
}

select +(select($pm), $~ = 'PODTURES')[0];
format PODTURES =
  ^<<<<<<<< ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<~~
$::bundle, $::feature
.

for ('default', sort grep /\.\d[02468]/, keys %feature_bundle) {
    $::bundle = ":$_";
    $::feature = join ' ', @{$feature_bundle{$_}};
    write $pm;
    print $pm "\n";
}

while (<DATA>) {
    print $pm $_ ;
}

read_only_bottom_close_and_rename($pm);


###########################################################################
# Generate feature.h

print $h <<EOH;

#if defined(PERL_CORE) || defined (PERL_EXT)

#define HINT_FEATURE_SHIFT	$HintShift

EOH

my $count;
for (@HintedBundles) {
    (my $key = uc) =~ y/.//d;
    print $h "#define FEATURE_BUNDLE_$key	", $count++, "\n";
}

print $h <<'EOH';
#define FEATURE_BUNDLE_CUSTOM	(HINT_FEATURE_MASK >> HINT_FEATURE_SHIFT)

#define CURRENT_HINTS \
    (PL_curcop == &PL_compiling ? PL_hints : PL_curcop->cop_hints)
#define CURRENT_FEATURE_BUNDLE \
    ((CURRENT_HINTS & HINT_FEATURE_MASK) >> HINT_FEATURE_SHIFT)

/* Avoid using ... && Perl_feature_is_enabled(...) as that triggers a bug in
   the HP-UX cc on PA-RISC */
#define FEATURE_IS_ENABLED(name)				        \
	((CURRENT_HINTS							 \
	   & HINT_LOCALIZE_HH)						  \
	    ? Perl_feature_is_enabled(aTHX_ STR_WITH_LEN(name)) : FALSE)
/* The longest string we pass in.  */
EOH

my $longest_internal_feature_name = longest values %feature;
print $h <<EOL;
#define MAX_FEATURE_LEN (sizeof("$longest_internal_feature_name")-1)

EOL

for (
    sort { length $a <=> length $b || $a cmp $b } keys %feature
) {
    my($first,$last) =
	map { (my $__ = uc) =~ y/.//d; $__ } @{$BundleRanges{$_}};
    my $name = $feature{$_};
    my $NAME = uc $name;
    if ($last && $first eq 'DEFAULT') { #  ‘>= DEFAULT’ warns
	print $h <<EOI;
#define FEATURE_$NAME\_IS_ENABLED \\
    ( \\
	CURRENT_FEATURE_BUNDLE <= FEATURE_BUNDLE_$last \\
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \\
	 FEATURE_IS_ENABLED("$name")) \\
    )

EOI
    }
    elsif ($last) {
	print $h <<EOH3;
#define FEATURE_$NAME\_IS_ENABLED \\
    ( \\
	(CURRENT_FEATURE_BUNDLE >= FEATURE_BUNDLE_$first && \\
	 CURRENT_FEATURE_BUNDLE <= FEATURE_BUNDLE_$last) \\
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \\
	 FEATURE_IS_ENABLED("$name")) \\
    )

EOH3
    }
    elsif ($first) {
	print $h <<EOH4;
#define FEATURE_$NAME\_IS_ENABLED \\
    ( \\
	CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_$first \\
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \\
	 FEATURE_IS_ENABLED("$name")) \\
    )

EOH4
    }
    else {
	print $h <<EOH5;
#define FEATURE_$NAME\_IS_ENABLED \\
    ( \\
	CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \\
	 FEATURE_IS_ENABLED("$name") \\
    )

EOH5
    }
}

print $h <<EOH;

#endif /* PERL_CORE or PERL_EXT */

#ifdef PERL_IN_OP_C
PERL_STATIC_INLINE void
S_enable_feature_bundle(pTHX_ SV *ver)
{
    SV *comp_ver = sv_newmortal();
    PL_hints = (PL_hints &~ HINT_FEATURE_MASK)
	     | (
EOH

for (reverse @HintedBundles[1..$#HintedBundles]) { # skip default
    my $numver = $_;
    if ($numver eq '5.10') { $numver = '5.009005' } # special case
    else		   { $numver =~ s/\./.0/  } # 5.11 => 5.011
    (my $macrover = $_) =~ y/.//d;
    print $h <<"    EOK";
		  (sv_setnv(comp_ver, $numver),
		   vcmp(ver, upg_version(comp_ver, FALSE)) >= 0)
			? FEATURE_BUNDLE_$macrover :
    EOK
}

print $h <<EOJ;
			  FEATURE_BUNDLE_DEFAULT
	       ) << HINT_FEATURE_SHIFT;
    /* special case */
    assert(PL_curcop == &PL_compiling);
    if (FEATURE_UNICODE_IS_ENABLED) PL_hints |=  HINT_UNI_8_BIT;
    else			    PL_hints &= ~HINT_UNI_8_BIT;
}
#endif /* PERL_IN_OP_C */
EOJ

read_only_bottom_close_and_rename($h);


###########################################################################
# Template for feature.pm

__END__
package feature;

our $VERSION = '1.36';

FEATURES

# TODO:
# - think about versioned features (use feature switch => 2)

=head1 NAME

feature - Perl pragma to enable new features

=head1 SYNOPSIS

    use feature qw(say switch);
    given ($foo) {
        when (1)          { say "\$foo == 1" }
        when ([2,3])      { say "\$foo == 2 || \$foo == 3" }
        when (/^a[bc]d$/) { say "\$foo eq 'abd' || \$foo eq 'acd'" }
        when ($_ > 100)   { say "\$foo > 100" }
        default           { say "None of the above" }
    }

    use feature ':5.10'; # loads all features available in perl 5.10

    use v5.10;           # implicitly loads :5.10 feature bundle

=head1 DESCRIPTION

It is usually impossible to add new syntax to Perl without breaking
some existing programs.  This pragma provides a way to minimize that
risk. New syntactic constructs, or new semantic meanings to older
constructs, can be enabled by C<use feature 'foo'>, and will be parsed
only when the appropriate feature pragma is in scope.  (Nevertheless, the
C<CORE::> prefix provides access to all Perl keywords, regardless of this
pragma.)

=head2 Lexical effect

Like other pragmas (C<use strict>, for example), features have a lexical
effect.  C<use feature qw(foo)> will only make the feature "foo" available
from that point to the end of the enclosing block.

    {
        use feature 'say';
        say "say is available here";
    }
    print "But not here.\n";

=head2 C<no feature>

Features can also be turned off by using C<no feature "foo">.  This too
has lexical effect.

    use feature 'say';
    say "say is available here";
    {
        no feature 'say';
        print "But not here.\n";
    }
    say "Yet it is here.";

C<no feature> with no features specified will reset to the default group.  To
disable I<all> features (an unusual request!) use C<no feature ':all'>.

=head1 AVAILABLE FEATURES

=head2 The 'say' feature

C<use feature 'say'> tells the compiler to enable the Perl 6 style
C<say> function.

See L<perlfunc/say> for details.

This feature is available starting with Perl 5.10.

=head2 The 'state' feature

C<use feature 'state'> tells the compiler to enable C<state>
variables.

See L<perlsub/"Persistent Private Variables"> for details.

This feature is available starting with Perl 5.10.

=head2 The 'switch' feature

C<use feature 'switch'> tells the compiler to enable the Perl 6
given/when construct.

See L<perlsyn/"Switch Statements"> for details.

This feature is available starting with Perl 5.10.

=head2 The 'unicode_strings' feature

C<use feature 'unicode_strings'> tells the compiler to use Unicode rules
in all string operations executed within its scope (unless they are also
within the scope of either C<use locale> or C<use bytes>).  The same applies
to all regular expressions compiled within the scope, even if executed outside
it.  It does not change the internal representation of strings, but only how
they are interpreted.

C<no feature 'unicode_strings'> tells the compiler to use the traditional
Perl rules wherein the native character set rules is used unless it is
clear to Perl that Unicode is desired.  This can lead to some surprises
when the behavior suddenly changes.  (See
L<perlunicode/The "Unicode Bug"> for details.)  For this reason, if you are
potentially using Unicode in your program, the
C<use feature 'unicode_strings'> subpragma is B<strongly> recommended.

This feature is available starting with Perl 5.12; was almost fully
implemented in Perl 5.14; and extended in Perl 5.16 to cover C<quotemeta>.

=head2 The 'unicode_eval' and 'evalbytes' features

Under the C<unicode_eval> feature, Perl's C<eval> function, when passed a
string, will evaluate it as a string of characters, ignoring any
C<use utf8> declarations.  C<use utf8> exists to declare the encoding of
the script, which only makes sense for a stream of bytes, not a string of
characters.  Source filters are forbidden, as they also really only make
sense on strings of bytes.  Any attempt to activate a source filter will
result in an error.

The C<evalbytes> feature enables the C<evalbytes> keyword, which evaluates
the argument passed to it as a string of bytes.  It dies if the string
contains any characters outside the 8-bit range.  Source filters work
within C<evalbytes>: they apply to the contents of the string being
evaluated.

Together, these two features are intended to replace the historical C<eval>
function, which has (at least) two bugs in it, that cannot easily be fixed
without breaking existing programs:

=over

=item *

C<eval> behaves differently depending on the internal encoding of the
string, sometimes treating its argument as a string of bytes, and sometimes
as a string of characters.

=item *

Source filters activated within C<eval> leak out into whichever I<file>
scope is currently being compiled.  To give an example with the CPAN module
L<Semi::Semicolons>:

    BEGIN { eval "use Semi::Semicolons;  # not filtered here " }
    # filtered here!

C<evalbytes> fixes that to work the way one would expect:

    use feature "evalbytes";
    BEGIN { evalbytes "use Semi::Semicolons;  # filtered " }
    # not filtered

=back

These two features are available starting with Perl 5.16.

=head2 The 'current_sub' feature

This provides the C<__SUB__> token that returns a reference to the current
subroutine or C<undef> outside of a subroutine.

This feature is available starting with Perl 5.16.

=head2 The 'array_base' feature

This feature supports the legacy C<$[> variable.  See L<perlvar/$[> and
L<arybase>.  It is on by default but disabled under C<use v5.16> (see
L</IMPLICIT LOADING>, below).

This feature is available under this name starting with Perl 5.16.  In
previous versions, it was simply on all the time, and this pragma knew
nothing about it.

=head2 The 'fc' feature

C<use feature 'fc'> tells the compiler to enable the C<fc> function,
which implements Unicode casefolding.

See L<perlfunc/fc> for details.

This feature is available from Perl 5.16 onwards.

=head2 The 'lexical_subs' feature

B<WARNING>: This feature is still experimental and the implementation may
change in future versions of Perl.  For this reason, Perl will
warn when you use the feature, unless you have explicitly disabled the
warning:

    no warnings "experimental::lexical_subs";

This enables declaration of subroutines via C<my sub foo>, C<state sub foo>
and C<our sub foo> syntax.  See L<perlsub/Lexical Subroutines> for details.

This feature is available from Perl 5.18 onwards.

=head2 The 'signatures' feature

B<WARNING>: This feature is still experimental and the implementation may
change in future versions of Perl.  For this reason, Perl will
warn when you use the feature, unless you have explicitly disabled the
warning:

    no warnings "experimental::signatures";

This enables unpacking of subroutine arguments into lexical variables
by syntax such as

    sub foo ($left, $right) {
	return $left + $right;
    }

See L<perlsub/Signatures> for details.

This feature is available from Perl 5.20 onwards.

=head1 FEATURE BUNDLES

It's possible to load multiple features together, using
a I<feature bundle>.  The name of a feature bundle is prefixed with
a colon, to distinguish it from an actual feature.

  use feature ":5.10";

The following feature bundles are available:

  bundle    features included
  --------- -----------------
PODTURES
The C<:default> bundle represents the feature set that is enabled before
any C<use feature> or C<no feature> declaration.

Specifying sub-versions such as the C<0> in C<5.14.0> in feature bundles has
no effect.  Feature bundles are guaranteed to be the same for all sub-versions.

  use feature ":5.14.0";    # same as ":5.14"
  use feature ":5.14.1";    # same as ":5.14"

=head1 IMPLICIT LOADING

Instead of loading feature bundles by name, it is easier to let Perl do
implicit loading of a feature bundle for you.

There are two ways to load the C<feature> pragma implicitly:

=over 4

=item *

By using the C<-E> switch on the Perl command-line instead of C<-e>.
That will enable the feature bundle for that version of Perl in the
main compilation unit (that is, the one-liner that follows C<-E>).

=item *

By explicitly requiring a minimum Perl version number for your program, with
the C<use VERSION> construct.  That is,

    use v5.10.0;

will do an implicit

    no feature ':all';
    use feature ':5.10';

and so on.  Note how the trailing sub-version
is automatically stripped from the
version.

But to avoid portability warnings (see L<perlfunc/use>), you may prefer:

    use 5.010;

with the same effect.

If the required version is older than Perl 5.10, the ":default" feature
bundle is automatically loaded instead.

=back

=cut

sub import {
    my $class = shift;

    if (!@_) {
        croak("No features specified");
    }

    __common(1, @_);
}

sub unimport {
    my $class = shift;

    # A bare C<no feature> should reset to the default bundle
    if (!@_) {
	$^H &= ~($hint_uni8bit|$hint_mask);
	return;
    }

    __common(0, @_);
}


sub __common {
    my $import = shift;
    my $bundle_number = $^H & $hint_mask;
    my $features = $bundle_number != $hint_mask
	&& $feature_bundle{$hint_bundles[$bundle_number >> $hint_shift]};
    if ($features) {
	# Features are enabled implicitly via bundle hints.
	# Delete any keys that may be left over from last time.
	delete @^H{ values(%feature) };
	$^H |= $hint_mask;
	for (@$features) {
	    $^H{$feature{$_}} = 1;
	    $^H |= $hint_uni8bit if $_ eq 'unicode_strings';
	}
    }
    while (@_) {
        my $name = shift;
        if (substr($name, 0, 1) eq ":") {
            my $v = substr($name, 1);
            if (!exists $feature_bundle{$v}) {
                $v =~ s/^([0-9]+)\.([0-9]+).[0-9]+$/$1.$2/;
                if (!exists $feature_bundle{$v}) {
                    unknown_feature_bundle(substr($name, 1));
                }
            }
            unshift @_, @{$feature_bundle{$v}};
            next;
        }
        if (!exists $feature{$name}) {
            unknown_feature($name);
        }
	if ($import) {
	    $^H{$feature{$name}} = 1;
	    $^H |= $hint_uni8bit if $name eq 'unicode_strings';
	} else {
            delete $^H{$feature{$name}};
            $^H &= ~ $hint_uni8bit if $name eq 'unicode_strings';
        }
    }
}

sub unknown_feature {
    my $feature = shift;
    croak(sprintf('Feature "%s" is not supported by Perl %vd',
            $feature, $^V));
}

sub unknown_feature_bundle {
    my $feature = shift;
    croak(sprintf('Feature bundle "%s" is not supported by Perl %vd',
            $feature, $^V));
}

sub croak {
    require Carp;
    Carp::croak(@_);
}

1;
