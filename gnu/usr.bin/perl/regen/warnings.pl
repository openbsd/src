#!/usr/bin/perl
#
# Regenerate (overwriting only if changed):
#
#    lib/warnings.pm
#    warnings.h
#
# from information hardcoded into this script (the $tree hash), plus the
# template for warnings.pm in the DATA section.
#
# When changing the number of warnings, t/op/caller.t should change to
# correspond with the value of $BYTES in lib/warnings.pm
#
# With an argument of 'tree', just dump the contents of $tree and exits.
# Also accepts the standard regen_lib -q and -v args.
#
# This script is normally invoked from regen.pl.

$VERSION = '1.03';

BEGIN {
    require 'regen/regen_lib.pl';
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
				'syscalls'      => [ 5.019, DEFAULT_OFF],
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
                                'illegalproto'  => [ 5.011, DEFAULT_OFF],
			   }],
       	'severe'	=> [ 5.008, {
				'inplace'	=> [ 5.008, DEFAULT_ON],
	 			'internal'	=> [ 5.008, DEFAULT_OFF],
         			'debugging'	=> [ 5.008, DEFAULT_ON],
         			'malloc'	=> [ 5.008, DEFAULT_ON],
			   }],
        'deprecated'	=> [ 5.008, DEFAULT_ON],
       	'void'		=> [ 5.008, DEFAULT_OFF],
       	'recursion'	=> [ 5.008, DEFAULT_OFF],
       	'redefine'	=> [ 5.008, DEFAULT_OFF],
       	'numeric'	=> [ 5.008, DEFAULT_OFF],
        'uninitialized'	=> [ 5.008, DEFAULT_OFF],
       	'once'		=> [ 5.008, DEFAULT_OFF],
       	'misc'		=> [ 5.008, DEFAULT_OFF],
       	'regexp'	=> [ 5.008, DEFAULT_OFF],
       	'glob'		=> [ 5.008, DEFAULT_ON],
       	'untie'		=> [ 5.008, DEFAULT_OFF],
	'substr'	=> [ 5.008, DEFAULT_OFF],
	'taint'		=> [ 5.008, DEFAULT_OFF],
	'signal'	=> [ 5.008, DEFAULT_OFF],
	'closure'	=> [ 5.008, DEFAULT_OFF],
	'overflow'	=> [ 5.008, DEFAULT_OFF],
	'portable'	=> [ 5.008, DEFAULT_OFF],
	'utf8'		=> [ 5.008, {
                                'surrogate' => [ 5.013, DEFAULT_OFF],
                                'nonchar' => [ 5.013, DEFAULT_OFF],
                                'non_unicode' => [ 5.013, DEFAULT_OFF],
                        }],
       	'exiting'	=> [ 5.008, DEFAULT_OFF],
       	'pack'		=> [ 5.008, DEFAULT_OFF],
       	'unpack'	=> [ 5.008, DEFAULT_OFF],
       	'threads'	=> [ 5.008, DEFAULT_OFF],
       	'imprecision'	=> [ 5.011, DEFAULT_OFF],
       	'experimental'	=> [ 5.017, {
                                'experimental::lexical_subs' =>
                                    [ 5.017, DEFAULT_ON ],
                                'experimental::regex_sets' =>
                                    [ 5.017, DEFAULT_ON ],
                                'experimental::lexical_topic' =>
                                    [ 5.017, DEFAULT_ON ],
                                'experimental::smartmatch' =>
                                    [ 5.017, DEFAULT_ON ],
                                'experimental::postderef' =>
                                    [ 5.019, DEFAULT_ON ],
                                'experimental::autoderef' =>
                                    [ 5.019, DEFAULT_ON ],
                                'experimental::signatures' =>
                                    [ 5.019, DEFAULT_ON ],
                        }],

       	 #'default'	=> [ 5.008, DEFAULT_ON ],
  	}],
} ;

my @def ;
my %list ;
my %Value ;
my %ValueToName ;
my %NameToValue ;

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
	die "Can't find key '$k'"
	    if ! defined $NameToValue{uc $k} ;
        push @{ $list{$k} }, $NameToValue{uc $k} ;
	die "Value associated with key '$k' is not an ARRAY reference"
	    if !ref $v || ref $v ne 'ARRAY' ;

	my ($ver, $rest) = @{ $v } ;
	if (ref $rest)
	  { push (@{ $list{$k} }, walk ($rest)) }
	elsif ($rest == DEFAULT_ON)
	  { push @def, $NameToValue{uc $k} }

	push @list, @{ $list{$k} } ;
    }

   return @list ;
}

###########################################################################

sub mkRange
{
    my @a = @_ ;
    my @out = @a ;

    for my $i (1 .. @a - 1) {
      	$out[$i] = ".."
          if $a[$i] == $a[$i - 1] + 1
             && ($i >= @a  - 1 || $a[$i] + 1 == $a[$i + 1] );
    }
    $out[-1] = $a[-1] if $out[-1] eq "..";

    my $out = join(",",@out);

    $out =~ s/,(\.\.,)+/../g ;
    return $out;
}

###########################################################################
sub warningsTree
{
    my $tre = shift ;
    my $prefix = shift ;
    my ($k, $v) ;

    my $max = (sort {$a <=> $b} map { length $_ } keys %$tre)[-1] ;
    my @keys = sort keys %$tre ;

    my $rv = '';

    while ($k = shift @keys) {
	$v = $tre->{$k};
	die "Value associated with key '$k' is not an ARRAY reference"
	    if !ref $v || ref $v ne 'ARRAY' ;

        my $offset ;
	if ($tre ne $tree) {
	    $rv .= $prefix . "|\n" ;
	    $rv .= $prefix . "+- $k" ;
	    $offset = ' ' x ($max + 4) ;
	}
	else {
	    $rv .= $prefix . "$k" ;
	    $offset = ' ' x ($max + 1) ;
	}

	my ($ver, $rest) = @{ $v } ;
	if (ref $rest)
	{
	    my $bar = @keys ? "|" : " ";
	    $rv .= " -" . "-" x ($max - length $k ) . "+\n" ;
	    $rv .= warningsTree ($rest, $prefix . $bar . $offset )
	}
	else
	  { $rv .= "\n" }
    }

    return $rv;
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
    print warningsTree($tree, "    ") ;
    exit ;
}

my ($warn, $pm) = map {
    open_new($_, '>', { by => 'regen/warnings.pl' });
} 'warnings.h', 'lib/warnings.pm';

my ($index, $warn_size);

{
  # generate warnings.h

  print $warn <<'EOM';

#define Off(x)			((x) / 8)
#define Bit(x)			(1 << ((x) % 8))
#define IsSet(a, x)		((a)[Off(x)] & Bit(x))


#define G_WARN_OFF		0 	/* $^W == 0 */
#define G_WARN_ON		1	/* -w flag and $^W != 0 */
#define G_WARN_ALL_ON		2	/* -W flag */
#define G_WARN_ALL_OFF		4	/* -X flag */
#define G_WARN_ONCE		8	/* set if 'once' ever enabled */
#define G_WARN_ALL_MASK		(G_WARN_ALL_ON|G_WARN_ALL_OFF)

#define pWARN_STD		NULL
#define pWARN_ALL		(((STRLEN*)0)+1)    /* use warnings 'all' */
#define pWARN_NONE		(((STRLEN*)0)+2)    /* no  warnings 'all' */

#define specialWARN(x)		((x) == pWARN_STD || (x) == pWARN_ALL ||	\
				 (x) == pWARN_NONE)

/* if PL_warnhook is set to this value, then warnings die */
#define PERL_WARNHOOK_FATAL	(&PL_sv_placeholder)
EOM

  my $offset = 0 ;

  valueWalk ($tree) ;
  $index = orderValues();

  die <<EOM if $index > 255 ;
Too many warnings categories -- max is 255
    rewrite packWARN* & unpackWARN* macros
EOM

  walk ($tree) ;

  $index *= 2 ;
  $warn_size = int($index / 8) + ($index % 8 != 0) ;

  my $k ;
  my $last_ver = 0;
  foreach $k (sort { $a <=> $b } keys %ValueToName) {
      my ($name, $version) = @{ $ValueToName{$k} };
      print $warn "\n/* Warnings Categories added in Perl $version */\n\n"
          if $last_ver != $version ;
      $name =~ y/:/_/;
      print $warn tab(5, "#define WARN_$name"), " $k\n" ;
      $last_ver = $version ;
  }
  print $warn "\n" ;

  print $warn tab(5, '#define WARNsize'),	"$warn_size\n" ;
  print $warn tab(5, '#define WARN_ALLstring'), '"', ('\125' x $warn_size) , "\"\n" ;
  print $warn tab(5, '#define WARN_NONEstring'), '"', ('\0' x $warn_size) , "\"\n" ;

  print $warn <<'EOM';

#define isLEXWARN_on 	(PL_curcop->cop_warnings != pWARN_STD)
#define isLEXWARN_off	(PL_curcop->cop_warnings == pWARN_STD)
#define isWARN_ONCE	(PL_dowarn & (G_WARN_ON|G_WARN_ONCE))
#define isWARN_on(c,x)	(IsSet((U8 *)(c + 1), 2*(x)))
#define isWARNf_on(c,x)	(IsSet((U8 *)(c + 1), 2*(x)+1))

#define DUP_WARNINGS(p)		\
    (specialWARN(p) ? (STRLEN*)(p)	\
    : (STRLEN*)CopyD(p, PerlMemShared_malloc(sizeof(*p)+*p), sizeof(*p)+*p, \
		     			     char))

#define ckWARN(w)		Perl_ckwarn(aTHX_ packWARN(w))

/* The w1, w2 ... should be independent warnings categories; one shouldn't be
 * a subcategory of any other */

#define ckWARN2(w1,w2)		Perl_ckwarn(aTHX_ packWARN2(w1,w2))
#define ckWARN3(w1,w2,w3)	Perl_ckwarn(aTHX_ packWARN3(w1,w2,w3))
#define ckWARN4(w1,w2,w3,w4)	Perl_ckwarn(aTHX_ packWARN4(w1,w2,w3,w4))

#define ckWARN_d(w)		Perl_ckwarn_d(aTHX_ packWARN(w))
#define ckWARN2_d(w1,w2)	Perl_ckwarn_d(aTHX_ packWARN2(w1,w2))
#define ckWARN3_d(w1,w2,w3)	Perl_ckwarn_d(aTHX_ packWARN3(w1,w2,w3))
#define ckWARN4_d(w1,w2,w3,w4)	Perl_ckwarn_d(aTHX_ packWARN4(w1,w2,w3,w4))

#define WARNshift		8

#define packWARN(a)		(a                                      )

/* The a, b, ... should be independent warnings categories; one shouldn't be
 * a subcategory of any other */

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
EOM

  read_only_bottom_close_and_rename($warn);
}

while (<DATA>) {
    last if /^KEYWORDS$/ ;
    if ($_ eq "=for warnings.pl tree-goes-here\n") {
      print $pm warningsTree($tree, "    ");
      next;
    }
    print $pm $_ ;
}

my $last_ver = 0;
print $pm "our %Offsets = (\n" ;
foreach my $k (sort { $a <=> $b } keys %ValueToName) {
    my ($name, $version) = @{ $ValueToName{$k} };
    $name = lc $name;
    $k *= 2 ;
    if ( $last_ver != $version ) {
        print $pm "\n";
        print $pm tab(4, "    # Warnings Categories added in Perl $version");
        print $pm "\n\n";
    }
    print $pm tab(4, "    '$name'"), "=> $k,\n" ;
    $last_ver = $version;
}

print $pm "  );\n\n" ;

print $pm "our %Bits = (\n" ;
foreach my $k (sort keys  %list) {

    my $v = $list{$k} ;
    my @list = sort { $a <=> $b } @$v ;

    print $pm tab(4, "    '$k'"), '=> "',
		mkHex($warn_size, map $_ * 2 , @list),
		'", # [', mkRange(@list), "]\n" ;
}

print $pm "  );\n\n" ;

print $pm "our %DeadBits = (\n" ;
foreach my $k (sort keys  %list) {

    my $v = $list{$k} ;
    my @list = sort { $a <=> $b } @$v ;

    print $pm tab(4, "    '$k'"), '=> "',
		mkHex($warn_size, map $_ * 2 + 1 , @list),
		'", # [', mkRange(@list), "]\n" ;
}

print $pm "  );\n\n" ;
print $pm '$NONE     = "', ('\0' x $warn_size) , "\";\n" ;
print $pm '$DEFAULT  = "', mkHex($warn_size, map $_ * 2, @def),
			   '", # [', mkRange(@def), "]\n" ;
print $pm '$LAST_BIT = ' . "$index ;\n" ;
print $pm '$BYTES    = ' . "$warn_size ;\n" ;
while (<DATA>) {
    print $pm $_ ;
}

read_only_bottom_close_and_rename($pm);

__END__
package warnings;

our $VERSION = '1.23';

# Verify that we're called correctly so that warnings will work.
# see also strict.pm.
unless ( __FILE__ =~ /(^|[\/\\])\Q${\__PACKAGE__}\E\.pmc?$/ ) {
    my (undef, $f, $l) = caller;
    die("Incorrect use of pragma '${\__PACKAGE__}' at $f line $l.\n");
}

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

The C<warnings> pragma gives control over which warnings are enabled in
which parts of a Perl program.  It's a more flexible alternative for
both the command line flag B<-w> and the equivalent Perl variable,
C<$^W>.

This pragma works just like the C<strict> pragma.
This means that the scope of the warning pragma is limited to the
enclosing block.  It also means that the pragma setting will not
leak across files (via C<use>, C<require> or C<do>).  This allows
authors to independently define the degree of warning checks that will
be applied to their module.

By default, optional warnings are disabled, so any legacy code that
doesn't attempt to control the warnings will work unchanged.

All warnings are enabled in a block by either of these:

    use warnings;
    use warnings 'all';

Similarly all warnings are disabled in a block by either of these:

    no warnings;
    no warnings 'all';

For example, consider the code below:

    use warnings;
    my @a;
    {
        no warnings;
	my $b = @a[0];
    }
    my $c = @a[0];

The code in the enclosing block has warnings enabled, but the inner
block has them disabled.  In this case that means the assignment to the
scalar C<$c> will trip the C<"Scalar value @a[0] better written as $a[0]">
warning, but the assignment to the scalar C<$b> will not.

=head2 Default Warnings and Optional Warnings

Before the introduction of lexical warnings, Perl had two classes of
warnings: mandatory and optional. 

As its name suggests, if your code tripped a mandatory warning, you
would get a warning whether you wanted it or not.
For example, the code below would always produce an C<"isn't numeric">
warning about the "2:".

    my $a = "2:" + 3;

With the introduction of lexical warnings, mandatory warnings now become
I<default> warnings.  The difference is that although the previously
mandatory warnings are still enabled by default, they can then be
subsequently enabled or disabled with the lexical warning pragma.  For
example, in the code below, an C<"isn't numeric"> warning will only
be reported for the C<$a> variable.

    my $a = "2:" + 3;
    no warnings;
    my $b = "2:" + 3;

Note that neither the B<-w> flag or the C<$^W> can be used to
disable/enable default warnings.  They are still mandatory in this case.

=head2 What's wrong with B<-w> and C<$^W>

Although very useful, the big problem with using B<-w> on the command
line to enable warnings is that it is all or nothing.  Take the typical
scenario when you are writing a Perl program.  Parts of the code you
will write yourself, but it's very likely that you will make use of
pre-written Perl modules.  If you use the B<-w> flag in this case, you
end up enabling warnings in pieces of code that you haven't written.

Similarly, using C<$^W> to either disable or enable blocks of code is
fundamentally flawed.  For a start, say you want to disable warnings in
a block of code.  You might expect this to be enough to do the trick:

     {
         local ($^W) = 0;
	 my $a =+ 2;
	 my $b; chop $b;
     }

When this code is run with the B<-w> flag, a warning will be produced
for the C<$a> line:  C<"Reversed += operator">.

The problem is that Perl has both compile-time and run-time warnings.  To
disable compile-time warnings you need to rewrite the code like this:

     {
         BEGIN { $^W = 0 }
	 my $a =+ 2;
	 my $b; chop $b;
     }

The other big problem with C<$^W> is the way you can inadvertently
change the warning setting in unexpected places in your code.  For example,
when the code below is run (without the B<-w> flag), the second call
to C<doit> will trip a C<"Use of uninitialized value"> warning, whereas
the first will not.

    sub doit
    {
        my $b; chop $b;
    }

    doit();

    {
        local ($^W) = 1;
        doit()
    }

This is a side-effect of C<$^W> being dynamically scoped.

Lexical warnings get around these limitations by allowing finer control
over where warnings can or can't be tripped.

=head2 Controlling Warnings from the Command Line

There are three Command Line flags that can be used to control when
warnings are (or aren't) produced:

=over 5

=item B<-w>
X<-w>

This is  the existing flag.  If the lexical warnings pragma is B<not>
used in any of you code, or any of the modules that you use, this flag
will enable warnings everywhere.  See L<Backward Compatibility> for
details of how this flag interacts with lexical warnings.

=item B<-W>
X<-W>

If the B<-W> flag is used on the command line, it will enable all warnings
throughout the program regardless of whether warnings were disabled
locally using C<no warnings> or C<$^W =0>.
This includes all files that get
included via C<use>, C<require> or C<do>.
Think of it as the Perl equivalent of the "lint" command.

=item B<-X>
X<-X>

Does the exact opposite to the B<-W> flag, i.e. it disables all warnings.

=back

=head2 Backward Compatibility

If you are used to working with a version of Perl prior to the
introduction of lexically scoped warnings, or have code that uses both
lexical warnings and C<$^W>, this section will describe how they interact.

How Lexical Warnings interact with B<-w>/C<$^W>:

=over 5

=item 1.

If none of the three command line flags (B<-w>, B<-W> or B<-X>) that
control warnings is used and neither C<$^W> nor the C<warnings> pragma
are used, then default warnings will be enabled and optional warnings
disabled.
This means that legacy code that doesn't attempt to control the warnings
will work unchanged.

=item 2.

The B<-w> flag just sets the global C<$^W> variable as in 5.005.  This
means that any legacy code that currently relies on manipulating C<$^W>
to control warning behavior will still work as is. 

=item 3.

Apart from now being a boolean, the C<$^W> variable operates in exactly
the same horrible uncontrolled global way, except that it cannot
disable/enable default warnings.

=item 4.

If a piece of code is under the control of the C<warnings> pragma,
both the C<$^W> variable and the B<-w> flag will be ignored for the
scope of the lexical warning.

=item 5.

The only way to override a lexical warnings setting is with the B<-W>
or B<-X> command line flags.

=back

The combined effect of 3 & 4 is that it will allow code which uses
the C<warnings> pragma to control the warning behavior of $^W-type
code (using a C<local $^W=0>) if it really wants to, but not vice-versa.

=head2 Category Hierarchy
X<warning, categories>

A hierarchy of "categories" have been defined to allow groups of warnings
to be enabled/disabled in isolation.

The current hierarchy is:

=for warnings.pl tree-goes-here

Just like the "strict" pragma any of these categories can be combined

    use warnings qw(void redefine);
    no warnings qw(io syntax untie);

Also like the "strict" pragma, if there is more than one instance of the
C<warnings> pragma in a given scope the cumulative effect is additive. 

    use warnings qw(void); # only "void" warnings enabled
    ...
    use warnings qw(io);   # only "void" & "io" warnings enabled
    ...
    no warnings qw(void);  # only "io" warnings enabled

To determine which category a specific warning has been assigned to see
L<perldiag>.

Note: Before Perl 5.8.0, the lexical warnings category "deprecated" was a
sub-category of the "syntax" category.  It is now a top-level category
in its own right.

=head2 Fatal Warnings
X<warning, fatal>

The presence of the word "FATAL" in the category list will escalate any
warnings detected from the categories specified in the lexical scope
into fatal errors.  In the code below, the use of C<time>, C<length>
and C<join> can all produce a C<"Useless use of xxx in void context">
warning.

    use warnings;

    time;

    {
        use warnings FATAL => qw(void);
        length "abc";
    }

    join "", 1,2,3;

    print "done\n";

When run it produces this output

    Useless use of time in void context at fatal line 3.
    Useless use of length in void context at fatal line 7.  

The scope where C<length> is used has escalated the C<void> warnings
category into a fatal error, so the program terminates immediately when it
encounters the warning.

To explicitly turn off a "FATAL" warning you just disable the warning
it is associated with.  So, for example, to disable the "void" warning
in the example above, either of these will do the trick:

    no warnings qw(void);
    no warnings FATAL => qw(void);

If you want to downgrade a warning that has been escalated into a fatal
error back to a normal warning, you can use the "NONFATAL" keyword.  For
example, the code below will promote all warnings into fatal errors,
except for those in the "syntax" category.

    use warnings FATAL => 'all', NONFATAL => 'syntax';

As of Perl 5.20, instead of C<< use warnings FATAL => 'all'; >> you can
use:

   use v5.20;       # Perl 5.20 or greater is required for the following
   use warnings 'FATAL';  # short form of "use warnings FATAL => 'all';"

If you want your program to be compatible with versions of Perl before
5.20, you must use C<< use warnings FATAL => 'all'; >> instead.  (In
previous versions of Perl, the behavior of the statements
C<< use warnings 'FATAL'; >>, C<< use warnings 'NONFATAL'; >> and
C<< no warnings 'FATAL'; >> was unspecified; they did not behave as if
they included the C<< => 'all' >> portion.  As of 5.20, they do.)

B<NOTE:> Users of FATAL warnings, especially
those using C<< FATAL => 'all' >>
should be fully aware that they are risking future portability of their
programs by doing so.  Perl makes absolutely no commitments to not
introduce new warnings, or warnings categories in the future, and indeed
we explicitly reserve the right to do so.  Code that may not warn now may
warn in a future release of Perl if the Perl5 development team deems it
in the best interests of the community to do so.  Should code using FATAL
warnings break due to the introduction of a new warning we will NOT
consider it an incompatible change.  Users of FATAL warnings should take
special caution during upgrades to check to see if their code triggers
any new warnings and should pay particular attention to the fine print of
the documentation of the features they use to ensure they do not exploit
features that are documented as risky, deprecated, or unspecified, or where
the documentation says "so don't do that", or anything with the same sense
and spirit.  Use of such features in combination with FATAL warnings is
ENTIRELY AT THE USER'S RISK.

=head2 Reporting Warnings from a Module
X<warning, reporting> X<warning, registering>

The C<warnings> pragma provides a number of functions that are useful for
module authors.  These are used when you want to report a module-specific
warning to a calling module has enabled warnings via the C<warnings>
pragma.

Consider the module C<MyMod::Abc> below.

    package MyMod::Abc;

    use warnings::register;

    sub open {
        my $path = shift;
        if ($path !~ m#^/#) {
            warnings::warn("changing relative path to /var/abc")
                if warnings::enabled();
            $path = "/var/abc/$path";
        }
    }

    1;

The call to C<warnings::register> will create a new warnings category
called "MyMod::Abc", i.e. the new category name matches the current
package name.  The C<open> function in the module will display a warning
message if it gets given a relative path as a parameter.  This warnings
will only be displayed if the code that uses C<MyMod::Abc> has actually
enabled them with the C<warnings> pragma like below.

    use MyMod::Abc;
    use warnings 'MyMod::Abc';
    ...
    abc::open("../fred.txt");

It is also possible to test whether the pre-defined warnings categories are
set in the calling module with the C<warnings::enabled> function.  Consider
this snippet of code:

    package MyMod::Abc;

    sub open {
        warnings::warnif("deprecated", 
                         "open is deprecated, use new instead");
        new(@_);
    }

    sub new
    ...
    1;

The function C<open> has been deprecated, so code has been included to
display a warning message whenever the calling module has (at least) the
"deprecated" warnings category enabled.  Something like this, say.

    use warnings 'deprecated';
    use MyMod::Abc;
    ...
    MyMod::Abc::open($filename);

Either the C<warnings::warn> or C<warnings::warnif> function should be
used to actually display the warnings message.  This is because they can
make use of the feature that allows warnings to be escalated into fatal
errors.  So in this case

    use MyMod::Abc;
    use warnings FATAL => 'MyMod::Abc';
    ...
    MyMod::Abc::open('../fred.txt');

the C<warnings::warnif> function will detect this and die after
displaying the warning message.

The three warnings functions, C<warnings::warn>, C<warnings::warnif>
and C<warnings::enabled> can optionally take an object reference in place
of a category name.  In this case the functions will use the class name
of the object as the warnings category.

Consider this example:

    package Original;

    no warnings;
    use warnings::register;

    sub new
    {
        my $class = shift;
        bless [], $class;
    }

    sub check
    {
        my $self = shift;
        my $value = shift;

        if ($value % 2 && warnings::enabled($self))
          { warnings::warn($self, "Odd numbers are unsafe") }
    }

    sub doit
    {
        my $self = shift;
        my $value = shift;
        $self->check($value);
        # ...
    }

    1;

    package Derived;

    use warnings::register;
    use Original;
    our @ISA = qw( Original );
    sub new
    {
        my $class = shift;
        bless [], $class;
    }


    1;

The code below makes use of both modules, but it only enables warnings from 
C<Derived>.

    use Original;
    use Derived;
    use warnings 'Derived';
    my $a = Original->new();
    $a->doit(1);
    my $b = Derived->new();
    $a->doit(1);

When this code is run only the C<Derived> object, C<$b>, will generate
a warning. 

    Odd numbers are unsafe at main.pl line 7

Notice also that the warning is reported at the line where the object is first
used.

When registering new categories of warning, you can supply more names to
warnings::register like this:

    package MyModule;
    use warnings::register qw(format precision);

    ...

    warnings::warnif('MyModule::format', '...');

=head1 FUNCTIONS

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

=item warnings::fatal_enabled()

Return TRUE if the warnings category with the same name as the current
package has been set to FATAL in the calling module.
Otherwise returns FALSE.

=item warnings::fatal_enabled($category)

Return TRUE if the warnings category C<$category> has been set to FATAL in
the calling module.
Otherwise returns FALSE.

=item warnings::fatal_enabled($object)

Use the name of the class for the object reference, C<$object>, as the
warnings category.

Return TRUE if that warnings category has been set to FATAL in the first
scope where the object is used.
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

=item warnings::register_categories(@names)

This registers warning categories for the given names and is primarily for
use by the warnings::register pragma.

=back

See also L<perlmodlib/Pragmatic Modules> and L<perldiag>.

=cut

KEYWORDS

$All = "" ; vec($All, $Offsets{'all'}, 2) = 3 ;

sub Croaker
{
    require Carp; # this initializes %CarpInternal
    local $Carp::CarpInternal{'warnings'};
    delete $Carp::CarpInternal{'warnings'};
    Carp::croak(@_);
}

sub _bits {
    my $mask = shift ;
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

sub bits
{
    # called from B::Deparse.pm
    push @_, 'all' unless @_ ;
    return _bits(undef, @_) ;
}

sub import
{
    shift;

    my $mask = ${^WARNING_BITS} // ($^W ? $Bits{all} : $DEFAULT) ;

    if (vec($mask, $Offsets{'all'}, 1)) {
        $mask |= $Bits{'all'} ;
        $mask |= $DeadBits{'all'} if vec($mask, $Offsets{'all'}+1, 1);
    }

    # append 'all' when implied (after a lone "FATAL" or "NONFATAL")
    push @_, 'all' if @_==1 && ( $_[0] eq 'FATAL' || $_[0] eq 'NONFATAL' );

    # Empty @_ is equivalent to @_ = 'all' ;
    ${^WARNING_BITS} = @_ ? _bits($mask, @_) : $mask | $Bits{all} ;
}

sub unimport
{
    shift;

    my $catmask ;
    my $mask = ${^WARNING_BITS} // ($^W ? $Bits{all} : $DEFAULT) ;

    if (vec($mask, $Offsets{'all'}, 1)) {
        $mask |= $Bits{'all'} ;
        $mask |= $DeadBits{'all'} if vec($mask, $Offsets{'all'}+1, 1);
    }

    # append 'all' when implied (empty import list or after a lone "FATAL")
    push @_, 'all' if !@_ || @_==1 && $_[0] eq 'FATAL';

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

sub MESSAGE () { 4 };
sub FATAL () { 2 };
sub NORMAL () { 1 };

sub __chk
{
    my $category ;
    my $offset ;
    my $isobj = 0 ;
    my $wanted = shift;
    my $has_message = $wanted & MESSAGE;

    unless (@_ == 1 || @_ == ($has_message ? 2 : 0)) {
	my $sub = (caller 1)[3];
	my $syntax = $has_message ? "[category,] 'message'" : '[category]';
	Croaker("Usage: $sub($syntax)");
    }

    my $message = pop if $has_message;

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

    my $i;

    if ($isobj) {
        my $pkg;
        $i = 2;
        while (do { { package DB; $pkg = (caller($i++))[0] } } ) {
            last unless @DB::args && $DB::args[0] =~ /^$category=/ ;
        }
	$i -= 2 ;
    }
    else {
        $i = _error_loc(); # see where Carp will allocate the error
    }

    # Default to 0 if caller returns nothing.  Default to $DEFAULT if it
    # explicitly returns undef.
    my(@callers_bitmask) = (caller($i))[9] ;
    my $callers_bitmask =
	 @callers_bitmask ? $callers_bitmask[0] // $DEFAULT : 0 ;

    my @results;
    foreach my $type (FATAL, NORMAL) {
	next unless $wanted & $type;

	push @results, (vec($callers_bitmask, $offset + $type - 1, 1) ||
			vec($callers_bitmask, $Offsets{'all'} + $type - 1, 1));
    }

    # &enabled and &fatal_enabled
    return $results[0] unless $has_message;

    # &warnif, and the category is neither enabled as warning nor as fatal
    return if $wanted == (NORMAL | FATAL | MESSAGE)
	&& !($results[0] || $results[1]);

    require Carp;
    Carp::croak($message) if $results[0];
    # will always get here for &warn. will only get here for &warnif if the
    # category is enabled
    Carp::carp($message);
}

sub _mkMask
{
    my ($bit) = @_;
    my $mask = "";

    vec($mask, $bit, 1) = 1;
    return $mask;
}

sub register_categories
{
    my @names = @_;

    for my $name (@names) {
	if (! defined $Bits{$name}) {
	    $Bits{$name}     = _mkMask($LAST_BIT);
	    vec($Bits{'all'}, $LAST_BIT, 1) = 1;
	    $Offsets{$name}  = $LAST_BIT ++;
	    foreach my $k (keys %Bits) {
		vec($Bits{$k}, $LAST_BIT, 1) = 0;
	    }
	    $DeadBits{$name} = _mkMask($LAST_BIT);
	    vec($DeadBits{'all'}, $LAST_BIT++, 1) = 1;
	}
    }
}

sub _error_loc {
    require Carp;
    goto &Carp::short_error_loc; # don't introduce another stack frame
}

sub enabled
{
    return __chk(NORMAL, @_);
}

sub fatal_enabled
{
    return __chk(FATAL, @_);
}

sub warn
{
    return __chk(FATAL | MESSAGE, @_);
}

sub warnif
{
    return __chk(NORMAL | FATAL | MESSAGE, @_);
}

# These are not part of any public interface, so we can delete them to save
# space.
delete @warnings::{qw(NORMAL FATAL MESSAGE)};

1;
