#!/usr/bin/perl

$VERSION = '1.02_02';

BEGIN {
  push @INC, './lib';
}
use strict ;

sub DEFAULT_ON  () { 1 }
sub DEFAULT_OFF () { 2 }

my $tree = {

'all' => [ 5.008, {
	'io'		=> [ 5.008, { 	
				'pipe' 		=> [ 5.008, DEFAULT_OFF],
       				'unopened'	=> [ 5.008, DEFAULT_OFF],
       				'closed'	=> [ 5.008, DEFAULT_OFF],
       				'newline'	=> [ 5.008, DEFAULT_OFF],
       				'exec'		=> [ 5.008, DEFAULT_OFF],
       				'layer'		=> [ 5.008, DEFAULT_OFF],
			   }],
     	'syntax'	=> [ 5.008, { 	
				'ambiguous'	=> [ 5.008, DEFAULT_OFF],
			     	'semicolon'	=> [ 5.008, DEFAULT_OFF],
			     	'precedence'	=> [ 5.008, DEFAULT_OFF],
			     	'bareword'	=> [ 5.008, DEFAULT_OFF],
			     	'reserved'	=> [ 5.008, DEFAULT_OFF],
				'digit'		=> [ 5.008, DEFAULT_OFF],
			     	'parenthesis'	=> [ 5.008, DEFAULT_OFF],
       	 			'printf'	=> [ 5.008, DEFAULT_OFF],
       	 			'prototype'	=> [ 5.008, DEFAULT_OFF],
       	 			'qw'		=> [ 5.008, DEFAULT_OFF],
			   }],
       	'severe'	=> [ 5.008, { 	
				'inplace'	=> [ 5.008, DEFAULT_ON],
	 			'internal'	=> [ 5.008, DEFAULT_ON],
         			'debugging'	=> [ 5.008, DEFAULT_ON],
         			'malloc'	=> [ 5.008, DEFAULT_ON],
	 		   }],
        'deprecated'	=> [ 5.008, DEFAULT_OFF],
       	'void'		=> [ 5.008, DEFAULT_OFF],
       	'recursion'	=> [ 5.008, DEFAULT_OFF],
       	'redefine'	=> [ 5.008, DEFAULT_OFF],
       	'numeric'	=> [ 5.008, DEFAULT_OFF],
        'uninitialized'	=> [ 5.008, DEFAULT_OFF],
       	'once'		=> [ 5.008, DEFAULT_OFF],
       	'misc'		=> [ 5.008, DEFAULT_OFF],
       	'regexp'	=> [ 5.008, DEFAULT_OFF],
       	'glob'		=> [ 5.008, DEFAULT_OFF],
       	'y2k'		=> [ 5.008, DEFAULT_OFF],
       	'untie'		=> [ 5.008, DEFAULT_OFF],
	'substr'	=> [ 5.008, DEFAULT_OFF],
	'taint'		=> [ 5.008, DEFAULT_OFF],
	'signal'	=> [ 5.008, DEFAULT_OFF],
	'closure'	=> [ 5.008, DEFAULT_OFF],
	'overflow'	=> [ 5.008, DEFAULT_OFF],
	'portable'	=> [ 5.008, DEFAULT_OFF],
	'utf8'		=> [ 5.008, DEFAULT_OFF],
       	'exiting'	=> [ 5.008, DEFAULT_OFF],
       	'pack'		=> [ 5.008, DEFAULT_OFF],
       	'unpack'	=> [ 5.008, DEFAULT_OFF],
       	'threads'	=> [ 5.008, DEFAULT_OFF],
       	 #'default'	=> [ 5.008, DEFAULT_ON ],
  	}],
} ;

###########################################################################
sub tab {
    my($l, $t) = @_;
    $t .= "\t" x ($l - (length($t) + 1) / 8);
    $t;
}

###########################################################################

my %list ;
my %Value ;
my %ValueToName ;
my %NameToValue ;
my $index ;

my %v_list = () ;

sub valueWalk
{
    my $tre = shift ;
    my @list = () ;
    my ($k, $v) ;

    foreach $k (sort keys %$tre) {
	$v = $tre->{$k};
	die "duplicate key $k\n" if defined $list{$k} ;
	die "Value associated with key '$k' is not an ARRAY reference"
	    if !ref $v || ref $v ne 'ARRAY' ;

	my ($ver, $rest) = @{ $v } ;
	push @{ $v_list{$ver} }, $k;
	
	if (ref $rest)
	  { valueWalk ($rest) }

    }

}

sub orderValues
{
    my $index = 0;
    foreach my $ver ( sort { $a <=> $b } keys %v_list ) {
        foreach my $name (@{ $v_list{$ver} } ) {
	    $ValueToName{ $index } = [ uc $name, $ver ] ;
	    $NameToValue{ uc $name } = $index ++ ;
        }
    }

    return $index ;
}

###########################################################################

sub walk
{
    my $tre = shift ;
    my @list = () ;
    my ($k, $v) ;

    foreach $k (sort keys %$tre) {
	$v = $tre->{$k};
	die "duplicate key $k\n" if defined $list{$k} ;
	#$Value{$index} = uc $k ;
	die "Can't find key '$k'"
	    if ! defined $NameToValue{uc $k} ;
        push @{ $list{$k} }, $NameToValue{uc $k} ;
	die "Value associated with key '$k' is not an ARRAY reference"
	    if !ref $v || ref $v ne 'ARRAY' ;
	
	my ($ver, $rest) = @{ $v } ;
	if (ref $rest)
	  { push (@{ $list{$k} }, walk ($rest)) }

	push @list, @{ $list{$k} } ;
    }

   return @list ;
}

###########################################################################

sub mkRange
{
    my @a = @_ ;
    my @out = @a ;
    my $i ;


    for ($i = 1 ; $i < @a; ++ $i) {
      	$out[$i] = ".."
          if $a[$i] == $a[$i - 1] + 1 && $a[$i] + 1 == $a[$i + 1] ;
    }

    my $out = join(",",@out);

    $out =~ s/,(\.\.,)+/../g ;
    return $out;
}

###########################################################################
sub printTree
{
    my $tre = shift ;
    my $prefix = shift ;
    my ($k, $v) ;

    my $max = (sort {$a <=> $b} map { length $_ } keys %$tre)[-1] ;
    my @keys = sort keys %$tre ;

    while ($k = shift @keys) {
	$v = $tre->{$k};
	die "Value associated with key '$k' is not an ARRAY reference"
	    if !ref $v || ref $v ne 'ARRAY' ;
	
        my $offset ;
	if ($tre ne $tree) {
	    print $prefix . "|\n" ;
	    print $prefix . "+- $k" ;
	    $offset = ' ' x ($max + 4) ;
	}
	else {
	    print $prefix . "$k" ;
	    $offset = ' ' x ($max + 1) ;
	}

	my ($ver, $rest) = @{ $v } ;
	if (ref $rest)
	{
	    my $bar = @keys ? "|" : " ";
	    print " -" . "-" x ($max - length $k ) . "+\n" ;
	    printTree ($rest, $prefix . $bar . $offset )
	}
	else
	  { print "\n" }
    }

}

###########################################################################

sub mkHexOct
{
    my ($f, $max, @a) = @_ ;
    my $mask = "\x00" x $max ;
    my $string = "" ;

    foreach (@a) {
	vec($mask, $_, 1) = 1 ;
    }

    foreach (unpack("C*", $mask)) {
        if ($f eq 'x') {
            $string .= '\x' . sprintf("%2.2x", $_)
        }
        else {
            $string .= '\\' . sprintf("%o", $_)
        }
    }
    return $string ;
}

sub mkHex
{
    my($max, @a) = @_;
    return mkHexOct("x", $max, @a);
}

sub mkOct
{
    my($max, @a) = @_;
    return mkHexOct("o", $max, @a);
}

###########################################################################

if (@ARGV && $ARGV[0] eq "tree")
{
    printTree($tree, "    ") ;
    exit ;
}

unlink "warnings.h";
unlink "lib/warnings.pm";
open(WARN, ">warnings.h") || die "Can't create warnings.h: $!\n";
binmode WARN;
open(PM, ">lib/warnings.pm") || die "Can't create lib/warnings.pm: $!\n";
binmode PM;

print WARN <<'EOM' ;
/* -*- buffer-read-only: t -*-
   !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
   This file is built by warnings.pl
   Any changes made here will be lost!
*/


#define Off(x)			((x) / 8)
#define Bit(x)			(1 << ((x) % 8))
#define IsSet(a, x)		((a)[Off(x)] & Bit(x))


#define G_WARN_OFF		0 	/* $^W == 0 */
#define G_WARN_ON		1	/* -w flag and $^W != 0 */
#define G_WARN_ALL_ON		2	/* -W flag */
#define G_WARN_ALL_OFF		4	/* -X flag */
#define G_WARN_ONCE		8	/* set if 'once' ever enabled */
#define G_WARN_ALL_MASK		(G_WARN_ALL_ON|G_WARN_ALL_OFF)

#define pWARN_STD		Nullsv
#define pWARN_ALL		(Nullsv+1)	/* use warnings 'all' */
#define pWARN_NONE		(Nullsv+2)	/* no  warnings 'all' */

#define specialWARN(x)		((x) == pWARN_STD || (x) == pWARN_ALL ||	\
				 (x) == pWARN_NONE)
EOM

my $offset = 0 ;

$index = $offset ;
#@{ $list{"all"} } = walk ($tree) ;
valueWalk ($tree) ;
my $index = orderValues();

die <<EOM if $index > 255 ;
Too many warnings categories -- max is 255
    rewrite packWARN* & unpackWARN* macros 
EOM

walk ($tree) ;

$index *= 2 ;
my $warn_size = int($index / 8) + ($index % 8 != 0) ;

my $k ;
my $last_ver = 0;
foreach $k (sort { $a <=> $b } keys %ValueToName) {
    my ($name, $version) = @{ $ValueToName{$k} };
    print WARN "\n/* Warnings Categories added in Perl $version */\n\n"
        if $last_ver != $version ;
    print WARN tab(5, "#define WARN_$name"), "$k\n" ;
    $last_ver = $version ;
}
print WARN "\n" ;

print WARN tab(5, '#define WARNsize'),	"$warn_size\n" ;
#print WARN tab(5, '#define WARN_ALLstring'), '"', ('\377' x $warn_size) , "\"\n" ;
print WARN tab(5, '#define WARN_ALLstring'), '"', ('\125' x $warn_size) , "\"\n" ;
print WARN tab(5, '#define WARN_NONEstring'), '"', ('\0' x $warn_size) , "\"\n" ;
my $WARN_TAINTstring = mkOct($warn_size, map $_ * 2, @{ $list{'taint'} });

print WARN tab(5, '#define WARN_TAINTstring'), qq["$WARN_TAINTstring"\n] ;

print WARN <<'EOM';

#define isLEXWARN_on 	(PL_curcop->cop_warnings != pWARN_STD)
#define isLEXWARN_off	(PL_curcop->cop_warnings == pWARN_STD)
#define isWARN_ONCE	(PL_dowarn & (G_WARN_ON|G_WARN_ONCE))
#define isWARN_on(c,x)	(IsSet(SvPVX_const(c), 2*(x)))
#define isWARNf_on(c,x)	(IsSet(SvPVX_const(c), 2*(x)+1))

#define ckWARN(w)		Perl_ckwarn(aTHX_ packWARN(w))
#define ckWARN2(w1,w2)		Perl_ckwarn(aTHX_ packWARN2(w1,w2))
#define ckWARN3(w1,w2,w3)	Perl_ckwarn(aTHX_ packWARN3(w1,w2,w3))
#define ckWARN4(w1,w2,w3,w4)	Perl_ckwarn(aTHX_ packWARN4(w1,w2,w3,w4))

#define ckWARN_d(w)		Perl_ckwarn_d(aTHX_ packWARN(w))
#define ckWARN2_d(w1,w2)	Perl_ckwarn_d(aTHX_ packWARN2(w1,w2))
#define ckWARN3_d(w1,w2,w3)	Perl_ckwarn_d(aTHX_ packWARN3(w1,w2,w3))
#define ckWARN4_d(w1,w2,w3,w4)	Perl_ckwarn_d(aTHX_ packWARN4(w1,w2,w3,w4))

#define packWARN(a)		(a                                      )
#define packWARN2(a,b)		((a) | ((b)<<8)                         )
#define packWARN3(a,b,c)	((a) | ((b)<<8) | ((c)<<16)             )
#define packWARN4(a,b,c,d)	((a) | ((b)<<8) | ((c)<<16) | ((d) <<24))

#define unpackWARN1(x)		((x)        & 0xFF)
#define unpackWARN2(x)		(((x) >>8)  & 0xFF)
#define unpackWARN3(x)		(((x) >>16) & 0xFF)
#define unpackWARN4(x)		(((x) >>24) & 0xFF)

#define ckDEAD(x)							\
	   ( ! specialWARN(PL_curcop->cop_warnings) &&			\
	    ( isWARNf_on(PL_curcop->cop_warnings, WARN_ALL) || 		\
	      isWARNf_on(PL_curcop->cop_warnings, unpackWARN1(x)) ||	\
	      isWARNf_on(PL_curcop->cop_warnings, unpackWARN2(x)) ||	\
	      isWARNf_on(PL_curcop->cop_warnings, unpackWARN3(x)) ||	\
	      isWARNf_on(PL_curcop->cop_warnings, unpackWARN4(x))))

/* end of file warnings.h */
/* ex: set ro: */
EOM

close WARN ;

while (<DATA>) {
    last if /^KEYWORDS$/ ;
    print PM $_ ;
}

#$list{'all'} = [ $offset .. 8 * ($warn_size/2) - 1 ] ;

$last_ver = 0;
print PM "our %Offsets = (\n" ;
foreach my $k (sort { $a <=> $b } keys %ValueToName) {
    my ($name, $version) = @{ $ValueToName{$k} };
    $name = lc $name;
    $k *= 2 ;
    if ( $last_ver != $version ) {
        print PM "\n";
        print PM tab(4, "    # Warnings Categories added in Perl $version");
        print PM "\n\n";
    }
    print PM tab(4, "    '$name'"), "=> $k,\n" ;
    $last_ver = $version;
}

print PM "  );\n\n" ;

print PM "our %Bits = (\n" ;
foreach $k (sort keys  %list) {

    my $v = $list{$k} ;
    my @list = sort { $a <=> $b } @$v ;

    print PM tab(4, "    '$k'"), '=> "',
		# mkHex($warn_size, @list),
		mkHex($warn_size, map $_ * 2 , @list),
		'", # [', mkRange(@list), "]\n" ;
}

print PM "  );\n\n" ;

print PM "our %DeadBits = (\n" ;
foreach $k (sort keys  %list) {

    my $v = $list{$k} ;
    my @list = sort { $a <=> $b } @$v ;

    print PM tab(4, "    '$k'"), '=> "',
		# mkHex($warn_size, @list),
		mkHex($warn_size, map $_ * 2 + 1 , @list),
		'", # [', mkRange(@list), "]\n" ;
}

print PM "  );\n\n" ;
print PM '$NONE     = "', ('\0' x $warn_size) , "\";\n" ;
print PM '$LAST_BIT = ' . "$index ;\n" ;
print PM '$BYTES    = ' . "$warn_size ;\n" ;
while (<DATA>) {
    print PM $_ ;
}

print PM "# ex: set ro:\n";
close PM ;

__END__
# -*- buffer-read-only: t -*-
# !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
# This file was created by warnings.pl
# Any changes made here will be lost.
#

package warnings;

our $VERSION = '1.05';

=head1 NAME

warnings - Perl pragma to control optional warnings

=head1 SYNOPSIS

    use warnings;
    no warnings;

    use warnings "all";
    no warnings "all";

    use warnings::register;
    if (warnings::enabled()) {
        warnings::warn("some warning");
    }

    if (warnings::enabled("void")) {
        warnings::warn("void", "some warning");
    }

    if (warnings::enabled($object)) {
        warnings::warn($object, "some warning");
    }

    warnings::warnif("some warning");
    warnings::warnif("void", "some warning");
    warnings::warnif($object, "some warning");

=head1 DESCRIPTION

The C<warnings> pragma is a replacement for the command line flag C<-w>,
but the pragma is limited to the enclosing block, while the flag is global.
See L<perllexwarn> for more information.

If no import list is supplied, all possible warnings are either enabled
or disabled.

A number of functions are provided to assist module authors.

=over 4

=item use warnings::register

Creates a new warnings category with the same name as the package where
the call to the pragma is used.

=item warnings::enabled()

Use the warnings category with the same name as the current package.

Return TRUE if that warnings category is enabled in the calling module.
Otherwise returns FALSE.

=item warnings::enabled($category)

Return TRUE if the warnings category, C<$category>, is enabled in the
calling module.
Otherwise returns FALSE.

=item warnings::enabled($object)

Use the name of the class for the object reference, C<$object>, as the
warnings category.

Return TRUE if that warnings category is enabled in the first scope
where the object is used.
Otherwise returns FALSE.

=item warnings::warn($message)

Print C<$message> to STDERR.

Use the warnings category with the same name as the current package.

If that warnings category has been set to "FATAL" in the calling module
then die. Otherwise return.

=item warnings::warn($category, $message)

Print C<$message> to STDERR.

If the warnings category, C<$category>, has been set to "FATAL" in the
calling module then die. Otherwise return.

=item warnings::warn($object, $message)

Print C<$message> to STDERR.

Use the name of the class for the object reference, C<$object>, as the
warnings category.

If that warnings category has been set to "FATAL" in the scope where C<$object>
is first used then die. Otherwise return.


=item warnings::warnif($message)

Equivalent to:

    if (warnings::enabled())
      { warnings::warn($message) }

=item warnings::warnif($category, $message)

Equivalent to:

    if (warnings::enabled($category))
      { warnings::warn($category, $message) }

=item warnings::warnif($object, $message)

Equivalent to:

    if (warnings::enabled($object))
      { warnings::warn($object, $message) }

=back

See L<perlmodlib/Pragmatic Modules> and L<perllexwarn>.

=cut

use Carp ();

KEYWORDS

$All = "" ; vec($All, $Offsets{'all'}, 2) = 3 ;

sub Croaker
{
    local $Carp::CarpInternal{'warnings'};
    delete $Carp::CarpInternal{'warnings'};
    Carp::croak(@_);
}

sub bits
{
    # called from B::Deparse.pm

    push @_, 'all' unless @_;

    my $mask;
    my $catmask ;
    my $fatal = 0 ;
    my $no_fatal = 0 ;

    foreach my $word ( @_ ) {
	if ($word eq 'FATAL') {
	    $fatal = 1;
	    $no_fatal = 0;
	}
	elsif ($word eq 'NONFATAL') {
	    $fatal = 0;
	    $no_fatal = 1;
	}
	elsif ($catmask = $Bits{$word}) {
	    $mask |= $catmask ;
	    $mask |= $DeadBits{$word} if $fatal ;
	    $mask &= ~($DeadBits{$word}|$All) if $no_fatal ;
	}
	else
          { Croaker("Unknown warnings category '$word'")}
    }

    return $mask ;
}

sub import 
{
    shift;

    my $catmask ;
    my $fatal = 0 ;
    my $no_fatal = 0 ;

    my $mask = ${^WARNING_BITS} ;

    if (vec($mask, $Offsets{'all'}, 1)) {
        $mask |= $Bits{'all'} ;
        $mask |= $DeadBits{'all'} if vec($mask, $Offsets{'all'}+1, 1);
    }
    
    push @_, 'all' unless @_;

    foreach my $word ( @_ ) {
	if ($word eq 'FATAL') {
	    $fatal = 1;
	    $no_fatal = 0;
	}
	elsif ($word eq 'NONFATAL') {
	    $fatal = 0;
	    $no_fatal = 1;
	}
	elsif ($catmask = $Bits{$word}) {
	    $mask |= $catmask ;
	    $mask |= $DeadBits{$word} if $fatal ;
	    $mask &= ~($DeadBits{$word}|$All) if $no_fatal ;
	}
	else
          { Croaker("Unknown warnings category '$word'")}
    }

    ${^WARNING_BITS} = $mask ;
}

sub unimport 
{
    shift;

    my $catmask ;
    my $mask = ${^WARNING_BITS} ;

    if (vec($mask, $Offsets{'all'}, 1)) {
        $mask |= $Bits{'all'} ;
        $mask |= $DeadBits{'all'} if vec($mask, $Offsets{'all'}+1, 1);
    }

    push @_, 'all' unless @_;

    foreach my $word ( @_ ) {
	if ($word eq 'FATAL') {
	    next; 
	}
	elsif ($catmask = $Bits{$word}) {
	    $mask &= ~($catmask | $DeadBits{$word} | $All);
	}
	else
          { Croaker("Unknown warnings category '$word'")}
    }

    ${^WARNING_BITS} = $mask ;
}

my %builtin_type; @builtin_type{qw(SCALAR ARRAY HASH CODE REF GLOB LVALUE Regexp)} = ();

sub __chk
{
    my $category ;
    my $offset ;
    my $isobj = 0 ;

    if (@_) {
        # check the category supplied.
        $category = shift ;
        if (my $type = ref $category) {
            Croaker("not an object")
                if exists $builtin_type{$type};
	    $category = $type;
            $isobj = 1 ;
        }
        $offset = $Offsets{$category};
        Croaker("Unknown warnings category '$category'")
	    unless defined $offset;
    }
    else {
        $category = (caller(1))[0] ;
        $offset = $Offsets{$category};
        Croaker("package '$category' not registered for warnings")
	    unless defined $offset ;
    }

    my $this_pkg = (caller(1))[0] ;
    my $i = 2 ;
    my $pkg ;

    if ($isobj) {
        while (do { { package DB; $pkg = (caller($i++))[0] } } ) {
            last unless @DB::args && $DB::args[0] =~ /^$category=/ ;
        }
	$i -= 2 ;
    }
    else {
        for ($i = 2 ; $pkg = (caller($i))[0] ; ++ $i) {
            last if $pkg ne $this_pkg ;
        }
        $i = 2
            if !$pkg || $pkg eq $this_pkg ;
    }

    my $callers_bitmask = (caller($i))[9] ;
    return ($callers_bitmask, $offset, $i) ;
}

sub enabled
{
    Croaker("Usage: warnings::enabled([category])")
	unless @_ == 1 || @_ == 0 ;

    my ($callers_bitmask, $offset, $i) = __chk(@_) ;

    return 0 unless defined $callers_bitmask ;
    return vec($callers_bitmask, $offset, 1) ||
           vec($callers_bitmask, $Offsets{'all'}, 1) ;
}


sub warn
{
    Croaker("Usage: warnings::warn([category,] 'message')")
	unless @_ == 2 || @_ == 1 ;

    my $message = pop ;
    my ($callers_bitmask, $offset, $i) = __chk(@_) ;
    Carp::croak($message)
	if vec($callers_bitmask, $offset+1, 1) ||
	   vec($callers_bitmask, $Offsets{'all'}+1, 1) ;
    Carp::carp($message) ;
}

sub warnif
{
    Croaker("Usage: warnings::warnif([category,] 'message')")
	unless @_ == 2 || @_ == 1 ;

    my $message = pop ;
    my ($callers_bitmask, $offset, $i) = __chk(@_) ;

    return
        unless defined $callers_bitmask &&
            	(vec($callers_bitmask, $offset, 1) ||
            	vec($callers_bitmask, $Offsets{'all'}, 1)) ;

    Carp::croak($message)
	if vec($callers_bitmask, $offset+1, 1) ||
	   vec($callers_bitmask, $Offsets{'all'}+1, 1) ;

    Carp::carp($message) ;
}

1;
