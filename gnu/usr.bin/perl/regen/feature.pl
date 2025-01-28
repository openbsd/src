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
    push @INC, './lib';
    require './regen/regen_lib.pl';
    require './regen/HeaderParser.pm';
}

use strict;
use warnings;

###########################################################################
# Hand-editable data

# (feature name) => (internal name, used in %^H and macro names)
my %feature = (
    say                     => 'say',
    state                   => 'state',
    switch                  => 'switch',
    bitwise                 => 'bitwise',
    evalbytes               => 'evalbytes',
    current_sub             => '__SUB__',
    refaliasing             => 'refaliasing',
    postderef_qq            => 'postderef_qq',
    unicode_eval            => 'unieval',
    declared_refs           => 'myref',
    unicode_strings         => 'unicode',
    fc                      => 'fc',
    signatures              => 'signatures',
    isa                     => 'isa',
    indirect                => 'indirect',
    multidimensional        => 'multidimensional',
    bareword_filehandles    => 'bareword_filehandles',
    try                     => 'try',
    defer                   => 'defer',
    extra_paired_delimiters => 'more_delims',
    module_true             => 'module_true',
    class                   => 'class',
);

# NOTE: If a feature is ever enabled in a non-contiguous range of Perl
#       versions, any code below that uses %BundleRanges will have to
#       be changed to account.

# 5.odd implies the next 5.even, but an explicit 5.even can override it.

# features bundles
use constant V5_9_5 => sort qw{say state switch indirect multidimensional bareword_filehandles};
use constant V5_11  => sort ( +V5_9_5, qw{unicode_strings} );
use constant V5_15  => sort ( +V5_11, qw{unicode_eval evalbytes current_sub fc} );
use constant V5_23  => sort ( +V5_15, qw{postderef_qq} );
use constant V5_27  => sort ( +V5_23, qw{bitwise} );

use constant V5_35  => sort grep {; $_ ne 'switch'
                                 && $_ ne 'indirect'
                                 && $_ ne 'multidimensional' } +V5_27, qw{isa signatures};

use constant V5_37  => sort grep {; $_ ne 'bareword_filehandles' } +V5_35, qw{module_true};

use constant V5_39  => sort ( +V5_37, qw{try} );

#
# when updating features please also update the Pod entry for L</"FEATURES CHEAT SHEET">
#
my %feature_bundle = (
    all     => [ sort keys %feature ],
    default => [ qw{indirect multidimensional bareword_filehandles} ],
    # using 5.9.5 features bundle
    "5.9.5" => [ +V5_9_5 ],
    "5.10"  => [ +V5_9_5 ],
    # using 5.11 features bundle
    "5.11"  => [ +V5_11 ],
    "5.13"  => [ +V5_11 ],
    # using 5.15 features bundle
    "5.15"  => [ +V5_15 ],
    "5.17"  => [ +V5_15 ],
    "5.19"  => [ +V5_15 ],
    "5.21"  => [ +V5_15 ],
    # using 5.23 features bundle
    "5.23"  => [ +V5_23 ],
    "5.25"  => [ +V5_23 ],
    # using 5.27 features bundle
    "5.27"  => [ +V5_27 ],
    "5.29"  => [ +V5_27 ],
    "5.31"  => [ +V5_27 ],
    "5.33"  => [ +V5_27 ],
    # using 5.35 features bundle
    "5.35"  => [ +V5_35 ],
    # using 5.37 features bundle
    "5.37"  => [ +V5_37 ],
    # using 5.39 features bundle
    "5.39"  => [ +V5_39 ],
);

my @noops = qw( postderef lexical_subs );
my @removed = qw( array_base );


###########################################################################
# More data generated from the above

if (keys %feature > 32) {
    die "cop_features only has room for 32 features";
}

my %feature_bits;
my $mask = 1;
for my $feature (sort keys %feature) {
    $feature_bits{$feature} = $mask;
    $mask <<= 1;
}

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
my $hp = HeaderParser->new()->read_file("perl.h");

foreach my $line_data (@{$hp->lines}) {
    next unless $line_data->{type} eq "content"
            and $line_data->{sub_type} eq "#define";
    my $line = $line_data->{line};
    next unless $line=~/^\s*#\s*define\s+(HINT_FEATURE_MASK|HINT_UNI_8_BIT)/;
    my $is_u8b = $1 =~ 8;
    $line=~/(0x[A-Fa-f0-9]+)/ or die "No hex number in:\n\n$line\n ";
    if ($is_u8b) {
	$Uni8Bit = $1;
    }
    else {
	my $hex = $HintMask = $1;
	my $bits = sprintf "%b", oct $1;
	$bits =~ /^0*1+(0*)\z/
         or die "Non-contiguous bits in $bits (binary for $hex):\n\n$line\n ";
	$HintShift = length $1;
	my $bits_needed =
	    length sprintf "%b", scalar keys %UniqueBundles;
	$bits =~ /1{$bits_needed}/
	    or die "Not enough bits (need $bits_needed)"
                 . " in $bits (binary for $hex):\n\n$line\n ";
    }
    if ($Uni8Bit && $HintMask) { last }
}
die "No HINT_FEATURE_MASK defined in perl.h" unless $HintMask;
die "No HINT_UNI_8_BIT defined in perl.h"    unless $Uni8Bit;

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
my $bund_width = length longest values %UniqueBundles;
for( sort { $UniqueBundles{$a} cmp $UniqueBundles{$b} }
          keys %UniqueBundles ) {
    my $bund = $UniqueBundles{$_};
    print $pm qq'    "$bund"' . " "x($bund_width-length $bund)
	    . qq' => [qw($_)],\n';
}
print $pm ");\n\n";

for (sort keys %Aliases) {
    print $pm
	qq'\$feature_bundle{"$_"} = \$feature_bundle{"$Aliases{$_}"};\n';
};

print $pm "my \%noops = (\n";
print $pm "    $_ => 1,\n", for @noops;
print $pm ");\n";

print $pm "my \%removed = (\n";
print $pm "    $_ => 1,\n", for @removed;
print $pm ");\n";

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

#ifndef PERL_FEATURE_H_
#define PERL_FEATURE_H_

#if defined(PERL_CORE) || defined (PERL_EXT)

#define HINT_FEATURE_SHIFT	$HintShift

EOH

for (sort keys %feature_bits) {
    printf $h "#define FEATURE_%s_BIT%*s %#06x\n", uc($feature{$_}),
      $width-length($feature{$_}), "", $feature_bits{$_};
}
print $h "\n";

my $count;
for (@HintedBundles) {
    (my $key = uc) =~ y/.//d;
    print $h "#define FEATURE_BUNDLE_$key	", $count++, "\n";
}

print $h <<'EOH';
#define FEATURE_BUNDLE_CUSTOM	(HINT_FEATURE_MASK >> HINT_FEATURE_SHIFT)

/* this is preserved for testing and asserts */
#define OLD_CURRENT_HINTS \
    (PL_curcop == &PL_compiling ? PL_hints : PL_curcop->cop_hints)
/* this is the same thing, but simpler (no if) as PL_hints expands
   to PL_compiling.cop_hints */
#define CURRENT_HINTS \
    PL_curcop->cop_hints
#define CURRENT_FEATURE_BUNDLE \
    ((CURRENT_HINTS & HINT_FEATURE_MASK) >> HINT_FEATURE_SHIFT)

#define FEATURE_IS_ENABLED_MASK(mask)                   \
  ((CURRENT_HINTS & HINT_LOCALIZE_HH)                \
    ? (PL_curcop->cop_features & (mask)) : FALSE)

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
    if ($last && $first eq 'DEFAULT') { #  '>= DEFAULT' warns
	print $h <<EOI;
#define FEATURE_${NAME}_IS_ENABLED \\
    ( \\
	CURRENT_FEATURE_BUNDLE <= FEATURE_BUNDLE_$last \\
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \\
	 FEATURE_IS_ENABLED_MASK(FEATURE_${NAME}_BIT)) \\
    )

EOI
    }
    elsif ($last) {
	print $h <<EOH3;
#define FEATURE_${NAME}_IS_ENABLED \\
    ( \\
	(CURRENT_FEATURE_BUNDLE >= FEATURE_BUNDLE_$first && \\
	 CURRENT_FEATURE_BUNDLE <= FEATURE_BUNDLE_$last) \\
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \\
	 FEATURE_IS_ENABLED_MASK(FEATURE_${NAME}_BIT)) \\
    )

EOH3
    }
    elsif ($first) {
	print $h <<EOH4;
#define FEATURE_${NAME}_IS_ENABLED \\
    ( \\
	CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_$first \\
     || (CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \\
	 FEATURE_IS_ENABLED_MASK(FEATURE_${NAME}_BIT)) \\
    )

EOH4
    }
    else {
	print $h <<EOH5;
#define FEATURE_${NAME}_IS_ENABLED \\
    ( \\
	CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM && \\
	 FEATURE_IS_ENABLED_MASK(FEATURE_${NAME}_BIT) \\
    )

EOH5
    }
}

print $h <<EOH;

#define SAVEFEATUREBITS() SAVEI32(PL_compiling.cop_features)

#define CLEARFEATUREBITS() (PL_compiling.cop_features = 0)

#define FETCHFEATUREBITSHH(hh) S_fetch_feature_bits_hh(aTHX_ (hh))

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

#if defined(PERL_IN_MG_C) || defined(PERL_IN_PP_CTL_C)

#define magic_sethint_feature(keysv, keypv, keylen, valsv, valbool) \\
    S_magic_sethint_feature(aTHX_ (keysv), (keypv), (keylen), (valsv), (valbool))
PERL_STATIC_INLINE void
S_magic_sethint_feature(pTHX_ SV *keysv, const char *keypv, STRLEN keylen,
                        SV *valsv, bool valbool) {
    if (keysv)
      keypv = SvPV_const(keysv, keylen);

    if (memBEGINs(keypv, keylen, "feature_")) {
        const char *subf = keypv + (sizeof("feature_")-1);
        U32 mask = 0;
        switch (*subf) {
EOJ

my %pref;
for my $key (sort values %feature) {
    push @{$pref{substr($key, 0, 1)}}, $key;
}

for my $pref (sort keys %pref) {
    print $h <<EOS;
        case '$pref':
EOS
    my $first = 1;
    for my $subkey (@{$pref{$pref}}) {
        my $rest = substr($subkey, 1);
        my $if = $first ? "if" : "else if";
        print $h <<EOJ;
            $if (keylen == sizeof("feature_$subkey")-1
                 && memcmp(subf+1, "$rest", keylen - sizeof("feature_")) == 0) {
                mask = FEATURE_\U${subkey}\E_BIT;
                break;
            }
EOJ

        $first = 0;
    }
    print $h <<EOS;
            return;

EOS
}

print $h <<EOJ;
        default:
            return;
        }
        if (valsv ? SvTRUE(valsv) : valbool)
            PL_compiling.cop_features |= mask;
        else
            PL_compiling.cop_features &= ~mask;
    }
}
#endif /* PERL_IN_MG_C */

/* subject to change */
struct perl_feature_bit {
  const char *name;
  STRLEN namelen;
  U32 mask;
};

#ifdef PERL_IN_PP_CTL_C

static const struct perl_feature_bit
PL_feature_bits[] = {
EOJ
for my $key (sort keys %feature) {
    my $val = $feature{$key};
    print $h <<EOJ;
    {
        /* feature $key */
        "feature_$val",
        STRLENs("feature_$val"),
        FEATURE_\U$val\E_BIT
    },
EOJ
}

print $h <<EOJ;
    { NULL, 0, 0U }
};

PERL_STATIC_INLINE void
S_fetch_feature_bits_hh(pTHX_ HV *hh) {
    PL_compiling.cop_features = 0;

    const struct perl_feature_bit *fb = PL_feature_bits;
    while (fb->name) {
        SV **svp = hv_fetch(hh, fb->name, (I32)fb->namelen, 0);
        if (svp && SvTRUE(*svp))
               PL_compiling.cop_features |= fb->mask;
        ++fb;
    }
}

#endif

#endif /* PERL_FEATURE_H_ */
EOJ

read_only_bottom_close_and_rename($h);


###########################################################################
# Template for feature.pm

__END__
package feature;
our $VERSION = '1.89';

FEATURES

# TODO:
# - think about versioned features (use feature switch => 2)

=encoding utf8

=head1 NAME

feature - Perl pragma to enable new features

=head1 SYNOPSIS

    use feature qw(fc say);

    # Without the "use feature" above, this code would not be able to find
    # the built-ins "say" or "fc":
    say "The case-folded version of $x is: " . fc $x;


    # set features to match the :5.36 bundle, which may turn off or on
    # multiple features (see "FEATURE BUNDLES" below)
    use feature ':5.36';


    # implicitly loads :5.36 feature bundle
    use v5.36;

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

Read L</"FEATURE BUNDLES"> for the feature cheat sheet summary.

=head2 The 'say' feature

C<use feature 'say'> tells the compiler to enable the Raku-inspired
C<say> function.

See L<perlfunc/say> for details.

This feature is available starting with Perl 5.10.

=head2 The 'state' feature

C<use feature 'state'> tells the compiler to enable C<state>
variables.

See L<perlsub/"Persistent Private Variables"> for details.

This feature is available starting with Perl 5.10.

=head2 The 'switch' feature

B<WARNING>: This feature is still experimental and the implementation may
change or be removed in future versions of Perl.  For this reason, Perl will
warn when you use the feature, unless you have explicitly disabled the warning:

    no warnings "experimental::smartmatch";

C<use feature 'switch'> tells the compiler to enable the Raku
given/when construct.

See L<perlsyn/"Switch Statements"> for details.

This feature is available starting with Perl 5.10.
It is deprecated starting with Perl 5.38, and using
C<given>, C<when> or smartmatch will throw a warning.
It will be removed in Perl 5.42.

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
implemented in Perl 5.14; and extended in Perl 5.16 to cover C<quotemeta>;
was extended further in Perl 5.26 to cover L<the range
operator|perlop/Range Operators>; and was extended again in Perl 5.28 to
cover L<special-cased whitespace splitting|perlfunc/split>.

=head2 The 'unicode_eval' and 'evalbytes' features

Together, these two features are intended to replace the legacy string
C<eval> function, which behaves problematically in some instances.  They are
available starting with Perl 5.16, and are enabled by default by a
S<C<use 5.16>> or higher declaration.

C<unicode_eval> changes the behavior of plain string C<eval> to work more
consistently, especially in the Unicode world.  Certain (mis)behaviors
couldn't be changed without breaking some things that had come to rely on
them, so the feature can be enabled and disabled.  Details are at
L<perlfunc/Under the "unicode_eval" feature>.

C<evalbytes> is like string C<eval>, but it treats its argument as a byte
string. Details are at L<perlfunc/evalbytes EXPR>.  Without a
S<C<use feature 'evalbytes'>> nor a S<C<use v5.16>> (or higher) declaration in
the current scope, you can still access it by instead writing
C<CORE::evalbytes>.

=head2 The 'current_sub' feature

This provides the C<__SUB__> token that returns a reference to the current
subroutine or C<undef> outside of a subroutine.

This feature is available starting with Perl 5.16.

=head2 The 'array_base' feature

This feature supported the legacy C<$[> variable.  See L<perlvar/$[>.
It was on by default but disabled under C<use v5.16> (see
L</IMPLICIT LOADING>, below) and unavailable since perl 5.30.

This feature is available under this name starting with Perl 5.16.  In
previous versions, it was simply on all the time, and this pragma knew
nothing about it.

=head2 The 'fc' feature

C<use feature 'fc'> tells the compiler to enable the C<fc> function,
which implements Unicode casefolding.

See L<perlfunc/fc> for details.

This feature is available from Perl 5.16 onwards.

=head2 The 'lexical_subs' feature

In Perl versions prior to 5.26, this feature enabled
declaration of subroutines via C<my sub foo>, C<state sub foo>
and C<our sub foo> syntax.  See L<perlsub/Lexical Subroutines> for details.

This feature is available from Perl 5.18 onwards.  From Perl 5.18 to 5.24,
it was classed as experimental, and Perl emitted a warning for its
usage, except when explicitly disabled:

  no warnings "experimental::lexical_subs";

As of Perl 5.26, use of this feature no longer triggers a warning, though
the C<experimental::lexical_subs> warning category still exists (for
compatibility with code that disables it).  In addition, this syntax is
not only no longer experimental, but it is enabled for all Perl code,
regardless of what feature declarations are in scope.

=head2 The 'postderef' and 'postderef_qq' features

The 'postderef_qq' feature extends the applicability of L<postfix
dereference syntax|perlref/Postfix Dereference Syntax> so that
postfix array dereference, postfix scalar dereference, and
postfix array highest index access are available in double-quotish interpolations.
For example, it makes the following two statements equivalent:

  my $s = "[@{ $h->{a} }]";
  my $s = "[$h->{a}->@*]";

This feature is available from Perl 5.20 onwards. In Perl 5.20 and 5.22, it
was classed as experimental, and Perl emitted a warning for its
usage, except when explicitly disabled:

  no warnings "experimental::postderef";

As of Perl 5.24, use of this feature no longer triggers a warning, though
the C<experimental::postderef> warning category still exists (for
compatibility with code that disables it).

The 'postderef' feature was used in Perl 5.20 and Perl 5.22 to enable
postfix dereference syntax outside double-quotish interpolations. In those
versions, using it triggered the C<experimental::postderef> warning in the
same way as the 'postderef_qq' feature did. As of Perl 5.24, this syntax is
not only no longer experimental, but it is enabled for all Perl code,
regardless of what feature declarations are in scope.

=head2 The 'signatures' feature

This enables syntax for declaring subroutine arguments as lexical variables.
For example, for this subroutine:

    sub foo ($left, $right) {
        return $left + $right;
    }

Calling C<foo(3, 7)> will assign C<3> into C<$left> and C<7> into C<$right>.

See L<perlsub/Signatures> for details.

This feature is available from Perl 5.20 onwards. From Perl 5.20 to 5.34,
it was classed as experimental, and Perl emitted a warning for its usage,
except when explicitly disabled:

  no warnings "experimental::signatures";

As of Perl 5.36, use of this feature no longer triggers a warning, though the
C<experimental::signatures> warning category still exists (for compatibility
with code that disables it). This feature is now considered stable, and is
enabled automatically by C<use v5.36> (or higher).

=head2 The 'refaliasing' feature

B<WARNING>: This feature is still experimental and the implementation may
change or be removed in future versions of Perl.  For this reason, Perl will
warn when you use the feature, unless you have explicitly disabled the warning:

    no warnings "experimental::refaliasing";

This enables aliasing via assignment to references:

    \$a = \$b; # $a and $b now point to the same scalar
    \@a = \@b; #                     to the same array
    \%a = \%b;
    \&a = \&b;
    foreach \%hash (@array_of_hash_refs) {
        ...
    }

See L<perlref/Assigning to References> for details.

This feature is available from Perl 5.22 onwards.

=head2 The 'bitwise' feature

This makes the four standard bitwise operators (C<& | ^ ~>) treat their
operands consistently as numbers, and introduces four new dotted operators
(C<&. |. ^. ~.>) that treat their operands consistently as strings.  The
same applies to the assignment variants (C<&= |= ^= &.= |.= ^.=>).

See L<perlop/Bitwise String Operators> for details.

This feature is available from Perl 5.22 onwards.  Starting in Perl 5.28,
C<use v5.28> will enable the feature.  Before 5.28, it was still
experimental and would emit a warning in the "experimental::bitwise"
category.

=head2 The 'declared_refs' feature

B<WARNING>: This feature is still experimental and the implementation may
change or be removed in future versions of Perl.  For this reason, Perl will
warn when you use the feature, unless you have explicitly disabled the warning:

    no warnings "experimental::declared_refs";

This allows a reference to a variable to be declared with C<my>, C<state>,
or C<our>, or localized with C<local>.  It is intended mainly for use in
conjunction with the "refaliasing" feature.  See L<perlref/Declaring a
Reference to a Variable> for examples.

This feature is available from Perl 5.26 onwards.

=head2 The 'isa' feature

This allows the use of the C<isa> infix operator, which tests whether the
scalar given by the left operand is an object of the class given by the
right operand. See L<perlop/Class Instance Operator> for more details.

This feature is available from Perl 5.32 onwards.  From Perl 5.32 to 5.34,
it was classed as experimental, and Perl emitted a warning for its usage,
except when explicitly disabled:

    no warnings "experimental::isa";

As of Perl 5.36, use of this feature no longer triggers a warning (though the
C<experimental::isa> warning category still exists for compatibility with
code that disables it). This feature is now considered stable, and is enabled
automatically by C<use v5.36> (or higher).

=head2 The 'indirect' feature

This feature allows the use of L<indirect object
syntax|perlobj/Indirect Object Syntax> for method calls, e.g.  C<new
Foo 1, 2;>. It is enabled by default, but can be turned off to
disallow indirect object syntax.

This feature is available under this name from Perl 5.32 onwards. In
previous versions, it was simply on all the time.  To disallow (or
warn on) indirect object syntax on older Perls, see the L<indirect>
CPAN module.

=head2 The 'multidimensional' feature

This feature enables multidimensional array emulation, a perl 4 (or
earlier) feature that was used to emulate multidimensional arrays with
hashes.  This works by converting code like C<< $foo{$x, $y} >> into
C<< $foo{join($;, $x, $y)} >>.  It is enabled by default, but can be
turned off to disable multidimensional array emulation.

When this feature is disabled the syntax that is normally replaced
will report a compilation error.

This feature is available under this name from Perl 5.34 onwards. In
previous versions, it was simply on all the time.

You can use the L<multidimensional> module on CPAN to disable
multidimensional array emulation for older versions of Perl.

=head2 The 'bareword_filehandles' feature

This feature enables bareword filehandles for builtin functions
operations, a generally discouraged practice.  It is enabled by
default, but can be turned off to disable bareword filehandles, except
for the exceptions listed below.

The perl built-in filehandles C<STDIN>, C<STDOUT>, C<STDERR>, C<DATA>,
C<ARGV>, C<ARGVOUT> and the special C<_> are always enabled.

This feature is available under this name from Perl 5.34 onwards.  In
previous versions it was simply on all the time.

You can use the L<bareword::filehandles> module on CPAN to disable
bareword filehandles for older versions of perl.

=head2 The 'try' feature

B<WARNING>: This feature is still partly experimental, and the implementation
may change or be removed in future versions of Perl.

This feature enables the C<try> and C<catch> syntax, which allows exception
handling, where exceptions thrown from the body of the block introduced with
C<try> are caught by executing the body of the C<catch> block.

This feature is available starting in Perl 5.34. Before Perl 5.40 it was
classed as experimental, and Perl emitted a warning for its usage, except when
explicitly disabled:

    no warnings "experimental::try";

As of Perl 5.40, use of this feature without a C<finally> block no longer
triggers a warning.  The optional C<finally> block is still considered
experimental and emits a warning, except when explicitly disabled as above.

For more information, see L<perlsyn/"Try Catch Exception Handling">.

=head2 The 'defer' feature

B<WARNING>: This feature is still experimental and the implementation may
change or be removed in future versions of Perl.  For this reason, Perl will
warn when you use the feature, unless you have explicitly disabled the warning:

    no warnings "experimental::defer";

This feature enables the C<defer> block syntax, which allows a block of code
to be deferred until when the flow of control leaves the block which contained
it. For more details, see L<perlsyn/defer>.

This feature is available starting in Perl 5.36.

=head2 The 'extra_paired_delimiters' feature

B<WARNING>: This feature is still experimental and the implementation may
change or be removed in future versions of Perl.  For this reason, Perl will
warn when you use the feature, unless you have explicitly disabled the warning:

    no warnings "experimental::extra_paired_delimiters";

This feature enables the use of more paired string delimiters than the
traditional four, S<C<< <  > >>>, S<C<( )>>, S<C<{ }>>, and S<C<[ ]>>.  When
this feature is on, for example, you can say S<C<qrE<171>patE<187>>>.

As with any usage of non-ASCII delimiters in a UTF-8-encoded source file, you
will want to ensure the parser will decode the source code from UTF-8 bytes
with a declaration such as C<use utf8>.

This feature is available starting in Perl 5.36.

For a full list of the available characters, see
L<perlop/List of Extra Paired Delimiters>.

=head2 The 'module_true' feature

This feature removes the need to return a true value at the end of a module
loaded with C<require> or C<use>. Any errors during compilation will cause
failures, but reaching the end of the module when this feature is in effect
will prevent C<perl> from throwing an exception that the module "did not return
a true value".

=head2 The 'class' feature

B<WARNING>: This feature is still experimental and the implementation may
change or be removed in future versions of Perl.  For this reason, Perl will
warn when you use the feature, unless you have explicitly disabled the warning:

    no warnings "experimental::class";

This feature enables the C<class> block syntax and other associated keywords
which implement the "new" object system, previously codenamed "Corinna".

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

You can also do:

  use feature ":all";

or

  no feature ":all";

but the first may enable features in a later version of Perl that
change the meaning of your code, and the second may disable mechanisms
that are part of Perl's current behavior that have been turned into
features, just as C<indirect> and C<bareword_filehandles> were.

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

    use v5.36.0;

will do an implicit

    no feature ':all';
    use feature ':5.36';

and so on.  Note how the trailing sub-version
is automatically stripped from the
version.

But to avoid portability warnings (see L<perlfunc/use>), you may prefer:

    use 5.036;

with the same effect.

If the required version is older than Perl 5.10, the ":default" feature
bundle is automatically loaded instead.

Unlike C<use feature ":5.12">, saying C<use v5.12> (or any higher version)
also does the equivalent of C<use strict>; see L<perlfunc/use> for details.

=back

=head1 CHECKING FEATURES

C<feature> provides some simple APIs to check which features are enabled.

These functions cannot be imported and must be called by their fully
qualified names.  If you don't otherwise need to set a feature you will
need to ensure C<feature> is loaded with:

  use feature ();

=over

=item feature_enabled($feature)

=item feature_enabled($feature, $depth)

  package MyStandardEnforcer;
  use feature ();
  use Carp "croak";
  sub import {
    croak "disable indirect!" if feature::feature_enabled("indirect");
  }

Test whether a named feature is enabled at a given level in the call
stack, returning a true value if it is.  C<$depth> defaults to 1,
which checks the scope that called the scope calling
feature::feature_enabled().

croaks for an unknown feature name.

=item features_enabled()

=item features_enabled($depth)

  package ReportEnabledFeatures;
  use feature "say";
  sub import {
    say STDERR join " ", feature::features_enabled();
  }

Returns a list of the features enabled at a given level in the call
stack.  C<$depth> defaults to 1, which checks the scope that called
the scope calling feature::features_enabled().

=item feature_bundle()

=item feature_bundle($depth)

Returns the feature bundle, if any, selected at a given level in the
call stack.  C<$depth> defaults to 1, which checks the scope that called
the scope calling feature::feature_bundle().

Returns an undefined value if no feature bundle is selected in the
scope.

The bundle name returned will be for the earliest bundle matching the
selected bundle, so:

  use feature ();
  use v5.12;
  BEGIN { print feature::feature_bundle(0); }

will print C<5.11>.

This returns internal state, at this point C<use v5.12;> sets the
feature bundle, but C< use feature ":5.12"; > does not set the feature
bundle.  This may change in a future release of perl.

=back

=cut

sub import {
    shift;

    if (!@_) {
        croak("No features specified");
    }

    __common(1, @_);
}

sub unimport {
    shift;

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
            if (exists $noops{$name}) {
                next;
            }
            if (!$import && exists $removed{$name}) {
                next;
            }
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

sub features_enabled {
    my ($depth) = @_;

    $depth //= 1;
    my @frame = caller($depth+1)
      or return;
    my ($hints, $hinthash) = @frame[8, 10];

    my $bundle_number = $hints & $hint_mask;
    if ($bundle_number != $hint_mask) {
        return $feature_bundle{$hint_bundles[$bundle_number >> $hint_shift]}->@*;
    }
    else {
        my @features;
        for my $feature (sort keys %feature) {
            if ($hinthash->{$feature{$feature}}) {
                push @features, $feature;
            }
        }
        return @features;
    }
}

sub feature_enabled {
    my ($feature, $depth) = @_;

    $depth //= 1;
    my @frame = caller($depth+1)
      or return;
    my ($hints, $hinthash) = @frame[8, 10];

    my $hint_feature = $feature{$feature}
      or croak "Unknown feature $feature";
    my $bundle_number = $hints & $hint_mask;
    if ($bundle_number != $hint_mask) {
        my $bundle = $hint_bundles[$bundle_number >> $hint_shift];
        for my $bundle_feature ($feature_bundle{$bundle}->@*) {
            return 1 if $bundle_feature eq $feature;
        }
        return 0;
    }
    else {
        return $hinthash->{$hint_feature} // 0;
    }
}

sub feature_bundle {
    my $depth = shift;

    $depth //= 1;
    my @frame = caller($depth+1)
      or return;
    my $bundle_number = $frame[8] & $hint_mask;
    if ($bundle_number != $hint_mask) {
        return $hint_bundles[$bundle_number >> $hint_shift];
    }
    else {
        return undef;
    }
}

1;
