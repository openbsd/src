#!/usr/bin/perl -w
# 
# Regenerate (overwriting only if changed):
#
#    embed.h
#    embedvar.h
#    perlapi.c
#    perlapi.h
#    proto.h
#
# from information stored in
#
#    embed.fnc
#    intrpvar.h
#    perlvars.h
#    regen/opcodes
#
# Accepts the standard regen_lib -q and -v args.
#
# This script is normally invoked from regen.pl.

require 5.004;	# keep this compatible, an old perl is all we may have before
                # we build the new one

use strict;

BEGIN {
    # Get function prototypes
    require 'regen/regen_lib.pl';
    require 'regen/embed_lib.pl';
}

my $SPLINT = 0; # Turn true for experimental splint support http://www.splint.org
my $unflagged_pointers;

#
# See database of global and static function prototypes in embed.fnc
# This is used to generate prototype headers under various configurations,
# export symbols lists for different platforms, and macros to provide an
# implicit interpreter context argument.
#

sub full_name ($$) { # Returns the function name with potentially the
		     # prefixes 'S_' or 'Perl_'
    my ($func, $flags) = @_;

    return "S_$func" if $flags =~ /[si]/;
    return "Perl_$func" if $flags =~ /[bp]/;
    return $func;
}

sub open_print_header {
    my ($file, $quote) = @_;

    return open_new($file, '>',
		    { file => $file, style => '*', by => 'regen/embed.pl',
		      from => ['data in embed.fnc', 'regen/embed.pl',
			       'regen/opcodes', 'intrpvar.h', 'perlvars.h'],
		      final => "\nEdit those files and run 'make regen_headers' to effect changes.\n",
		      copyright => [1993 .. 2009], quote => $quote });
}

my ($embed, $core, $ext, $api) = setup_embed();

# generate proto.h
{
    my $pr = open_print_header("proto.h");
    print $pr "START_EXTERN_C\n";
    my $ret;

    foreach (@$embed) {
	if (@$_ == 1) {
	    print $pr "$_->[0]\n";
	    next;
	}

	my ($flags,$retval,$plain_func,@args) = @$_;
	my @nonnull;
	my $has_context = ( $flags !~ /n/ );
	my $never_returns = ( $flags =~ /r/ );
	my $commented_out = ( $flags =~ /m/ );
	my $binarycompat = ( $flags =~ /b/ );
	my $is_malloc = ( $flags =~ /a/ );
	my $can_ignore = ( $flags !~ /R/ ) && !$is_malloc;
	my @names_of_nn;
	my $func;

	if (! $can_ignore && $retval eq 'void') {
	    warn "It is nonsensical to require the return value of a void function ($plain_func) to be checked";
	}

	my $scope_type_flag_count = 0;
	$scope_type_flag_count++ if $flags =~ /s/;
	$scope_type_flag_count++ if $flags =~ /i/;
	$scope_type_flag_count++ if $flags =~ /p/;
	warn "$plain_func: i, p, and s flags are all mutually exclusive"
						   if $scope_type_flag_count > 1;
	my $splint_flags = "";
	if ( $SPLINT && !$commented_out ) {
	    $splint_flags .= '/*@noreturn@*/ ' if $never_returns;
	    if ($can_ignore && ($retval ne 'void') && ($retval !~ /\*/)) {
		$retval .= " /*\@alt void\@*/";
	    }
	}

	if ($flags =~ /([si])/) {
	    my $type;
	    if ($never_returns) {
		$type = $1 eq 's' ? "PERL_STATIC_NO_RET" : "PERL_STATIC_INLINE_NO_RET";
	    }
	    else {
		$type = $1 eq 's' ? "STATIC" : "PERL_STATIC_INLINE";
	    }
	    $retval = "$type $splint_flags$retval";
	}
	else {
	    if ($never_returns) {
		$retval = "PERL_CALLCONV_NO_RET $splint_flags$retval";
	    }
	    else {
		$retval = "PERL_CALLCONV $splint_flags$retval";
	    }
	}
	$func = full_name($plain_func, $flags);
	$ret = "$retval\t$func(";
	if ( $has_context ) {
	    $ret .= @args ? "pTHX_ " : "pTHX";
	}
	if (@args) {
	    my $n;
	    for my $arg ( @args ) {
		++$n;
		if ( $arg =~ /\*/ && $arg !~ /\b(NN|NULLOK)\b/ ) {
		    warn "$func: $arg needs NN or NULLOK\n";
		    ++$unflagged_pointers;
		}
		my $nn = ( $arg =~ s/\s*\bNN\b\s+// );
		push( @nonnull, $n ) if $nn;

		my $nullok = ( $arg =~ s/\s*\bNULLOK\b\s+// ); # strip NULLOK with no effect

		# Make sure each arg has at least a type and a var name.
		# An arg of "int" is valid C, but want it to be "int foo".
		my $temp_arg = $arg;
		$temp_arg =~ s/\*//g;
		$temp_arg =~ s/\s*\bstruct\b\s*/ /g;
		if ( ($temp_arg ne "...")
		     && ($temp_arg !~ /\w+\s+(\w+)(?:\[\d+\])?\s*$/) ) {
		    warn "$func: $arg ($n) doesn't have a name\n";
		}
		if ( $SPLINT && $nullok && !$commented_out ) {
		    $arg = '/*@null@*/ ' . $arg;
		}
		if (defined $1 && $nn && !($commented_out && !$binarycompat)) {
		    push @names_of_nn, $1;
		}
	    }
	    $ret .= join ", ", @args;
	}
	else {
	    $ret .= "void" if !$has_context;
	}
	$ret .= ")";
	my @attrs;
	if ( $flags =~ /r/ ) {
	    push @attrs, "__attribute__noreturn__";
	}
	if ( $flags =~ /D/ ) {
	    push @attrs, "__attribute__deprecated__";
	}
	if ( $is_malloc ) {
	    push @attrs, "__attribute__malloc__";
	}
	if ( !$can_ignore ) {
	    push @attrs, "__attribute__warn_unused_result__";
	}
	if ( $flags =~ /P/ ) {
	    push @attrs, "__attribute__pure__";
	}
	if( $flags =~ /f/ ) {
	    my $prefix	= $has_context ? 'pTHX_' : '';
	    my ($args, $pat);
	    if ($args[-1] eq '...') {
		$args	= scalar @args;
		$pat	= $args - 1;
		$args	= $prefix . $args;
	    }
	    else {
		# don't check args, and guess which arg is the pattern
		# (one of 'fmt', 'pat', 'f'),
		$args = 0;
		my @fmts = grep $args[$_] =~ /\b(f|pat|fmt)$/, 0..$#args;
		if (@fmts != 1) {
		    die "embed.pl: '$plain_func': can't determine pattern arg\n";
		}
		$pat = $fmts[0] + 1;
	    }
	    my $macro	= grep($_ == $pat, @nonnull)
				? '__attribute__format__'
				: '__attribute__format__null_ok__';
	    if ($plain_func =~ /strftime/) {
		push @attrs, sprintf "%s(__strftime__,%s1,0)", $macro, $prefix;
	    }
	    else {
		push @attrs, sprintf "%s(__printf__,%s%d,%s)", $macro,
				    $prefix, $pat, $args;
	    }
	}
	if ( @nonnull ) {
	    my @pos = map { $has_context ? "pTHX_$_" : $_ } @nonnull;
	    push @attrs, map { sprintf( "__attribute__nonnull__(%s)", $_ ) } @pos;
	}
	if ( @attrs ) {
	    $ret .= "\n";
	    $ret .= join( "\n", map { "\t\t\t$_" } @attrs );
	}
	$ret .= ";";
	$ret = "/* $ret */" if $commented_out;
	if (@names_of_nn) {
	    $ret .= "\n#define PERL_ARGS_ASSERT_\U$plain_func\E\t\\\n\t"
		. join '; ', map "assert($_)", @names_of_nn;
	}
	$ret .= @attrs ? "\n\n" : "\n";

	print $pr $ret;
    }

    print $pr <<'EOF';
#ifdef PERL_CORE
#  include "pp_proto.h"
#endif
END_EXTERN_C
EOF

    read_only_bottom_close_and_rename($pr);
}

warn "$unflagged_pointers pointer arguments to clean up\n" if $unflagged_pointers;

sub readvars {
    my ($file, $pre) = @_;
    local (*FILE, $_);
    my %seen;
    open(FILE, "< $file")
	or die "embed.pl: Can't open $file: $!\n";
    while (<FILE>) {
	s/[ \t]*#.*//;		# Delete comments.
	if (/PERLVARA?I?C?\($pre,\s*(\w+)/) {
	    warn "duplicate symbol $1 while processing $file line $.\n"
		if $seen{$1}++;
	}
    }
    close(FILE);
    return sort keys %seen;
}

my @intrp = readvars 'intrpvar.h','I';
my @globvar = readvars 'perlvars.h','G';

sub hide {
    my ($from, $to, $indent) = @_;
    $indent = '' unless defined $indent;
    my $t = int(length("$indent$from") / 8);
    "#${indent}define $from" . "\t" x ($t < 3 ? 3 - $t : 1) . "$to\n";
}

sub multon ($$$) {
    my ($sym,$pre,$ptr) = @_;
    hide("PL_$sym", "($ptr$pre$sym)");
}

my $em = open_print_header('embed.h');

print $em <<'END';
/* (Doing namespace management portably in C is really gross.) */

/* By defining PERL_NO_SHORT_NAMES (not done by default) the short forms
 * (like warn instead of Perl_warn) for the API are not defined.
 * Not defining the short forms is a good thing for cleaner embedding. */

#ifndef PERL_NO_SHORT_NAMES

/* Hide global symbols */

END

my @az = ('a'..'z');

sub embed_h {
    my ($guard, $funcs) = @_;
    print $em "$guard\n" if $guard;

    my $lines;
    foreach (@$funcs) {
	if (@$_ == 1) {
	    my $cond = $_->[0];
	    # Indent the conditionals if we are wrapped in an #if/#endif pair.
	    $cond =~ s/#(.*)/#  $1/ if $guard;
	    $lines .= "$cond\n";
	    next;
	}
	my $ret = "";
	my ($flags,$retval,$func,@args) = @$_;
	unless ($flags =~ /[om]/) {
	    my $args = scalar @args;
	    if ($flags =~ /n/) {
		$ret = hide($func, full_name($func, $flags));
	    }
	    elsif ($args and $args[$args-1] =~ /\.\.\./) {
		if ($flags =~ /p/) {
		    # we're out of luck for varargs functions under CPP
		    # So we can only do these macros for no implicit context:
		    $ret = "#ifndef PERL_IMPLICIT_CONTEXT\n"
			. hide($func, full_name($func, $flags)) . "#endif\n";
		}
	    }
	    else {
		my $alist = join(",", @az[0..$args-1]);
		$ret = "#define $func($alist)";
		my $t = int(length($ret) / 8);
		$ret .=  "\t" x ($t < 4 ? 4 - $t : 1);
		$ret .= full_name($func, $flags) . "(aTHX";
		$ret .= "_ " if $alist;
		$ret .= $alist . ")\n";
	    }
	}
	$lines .= $ret;
    }
    # Prune empty #if/#endif pairs.
    while ($lines =~ s/#\s*if[^\n]+\n#\s*endif\n//) {
    }
    # Merge adjacent blocks.
    while ($lines =~ s/(#ifndef PERL_IMPLICIT_CONTEXT
[^\n]+
)#endif
#ifndef PERL_IMPLICIT_CONTEXT
/$1/) {
    }

    print $em $lines;
    print $em "#endif\n" if $guard;
}

embed_h('', $api);
embed_h('#if defined(PERL_CORE) || defined(PERL_EXT)', $ext);
embed_h('#ifdef PERL_CORE', $core);

print $em <<'END';

#endif	/* #ifndef PERL_NO_SHORT_NAMES */

/* Compatibility stubs.  Compile extensions with -DPERL_NOCOMPAT to
   disable them.
 */

#if !defined(PERL_CORE)
#  define sv_setptrobj(rv,ptr,name)	sv_setref_iv(rv,name,PTR2IV(ptr))
#  define sv_setptrref(rv,ptr)		sv_setref_iv(rv,NULL,PTR2IV(ptr))
#endif

#if !defined(PERL_CORE) && !defined(PERL_NOCOMPAT)

/* Compatibility for various misnamed functions.  All functions
   in the API that begin with "perl_" (not "Perl_") take an explicit
   interpreter context pointer.
   The following are not like that, but since they had a "perl_"
   prefix in previous versions, we provide compatibility macros.
 */
#  define perl_atexit(a,b)		call_atexit(a,b)
END

foreach (@$embed) {
    my ($flags, $retval, $func, @args) = @$_;
    next unless $func;
    next unless $flags =~ /O/;

    my $alist = join ",", @az[0..$#args];
    my $ret = "#  define perl_$func($alist)";
    my $t = (length $ret) >> 3;
    $ret .=  "\t" x ($t < 5 ? 5 - $t : 1);
    print $em "$ret$func($alist)\n";
}

my @nocontext;
{
    my (%has_va, %has_nocontext);
    foreach (@$embed) {
	next unless @$_ > 1;
	++$has_va{$_->[2]} if $_->[-1] =~ /\.\.\./;
	++$has_nocontext{$1} if $_->[2] =~ /(.*)_nocontext/;
    }

    @nocontext = sort grep {
	$has_nocontext{$_}
	    && !/printf/ # Not clear to me why these are skipped but they are.
    } keys %has_va;
}

print $em <<'END';

/* varargs functions can't be handled with CPP macros. :-(
   This provides a set of compatibility functions that don't take
   an extra argument but grab the context pointer using the macro
   dTHX.
 */
#if defined(PERL_IMPLICIT_CONTEXT) && !defined(PERL_NO_SHORT_NAMES)
END

foreach (@nocontext) {
    print $em hide($_, "Perl_${_}_nocontext", "  ");
}

print $em <<'END';
#endif

#endif /* !defined(PERL_CORE) && !defined(PERL_NOCOMPAT) */

#if !defined(PERL_IMPLICIT_CONTEXT)
/* undefined symbols, point them back at the usual ones */
END

foreach (@nocontext) {
    print $em hide("Perl_${_}_nocontext", "Perl_$_", "  ");
}

print $em <<'END';
#endif
END

read_only_bottom_close_and_rename($em);

$em = open_print_header('embedvar.h');

print $em <<'END';
/* (Doing namespace management portably in C is really gross.) */

/*
   The following combinations of MULTIPLICITY and PERL_IMPLICIT_CONTEXT
   are supported:
     1) none
     2) MULTIPLICITY	# supported for compatibility
     3) MULTIPLICITY && PERL_IMPLICIT_CONTEXT

   All other combinations of these flags are errors.

   only #3 is supported directly, while #2 is a special
   case of #3 (supported by redefining vTHX appropriately).
*/

#if defined(MULTIPLICITY)
/* cases 2 and 3 above */

#  if defined(PERL_IMPLICIT_CONTEXT)
#    define vTHX	aTHX
#  else
#    define vTHX	PERL_GET_INTERP
#  endif

END

my $sym;

for $sym (@intrp) {
    if ($sym eq 'sawampersand') {
	print $em "#ifndef PL_sawampersand\n";
    }
    print $em multon($sym,'I','vTHX->');
    if ($sym eq 'sawampersand') {
	print $em "#endif\n";
    }
}

print $em <<'END';

#endif	/* MULTIPLICITY */

#if defined(PERL_GLOBAL_STRUCT)

END

for $sym (@globvar) {
    print $em "#ifdef OS2\n" if $sym eq 'sh_path';
    print $em multon($sym,   'G','my_vars->');
    print $em multon("G$sym",'', 'my_vars->');
    print $em "#endif\n" if $sym eq 'sh_path';
}

print $em <<'END';

#endif /* PERL_GLOBAL_STRUCT */
END

read_only_bottom_close_and_rename($em);

my $capih = open_print_header('perlapi.h');

print $capih <<'EOT';
/* declare accessor functions for Perl variables */
#ifndef __perlapi_h__
#define __perlapi_h__

#if defined (MULTIPLICITY) && defined (PERL_GLOBAL_STRUCT)

START_EXTERN_C

#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC
#define PERLVAR(p,v,t)	EXTERN_C t* Perl_##p##v##_ptr(pTHX);
#define PERLVARA(p,v,n,t)	typedef t PL_##v##_t[n];		\
			EXTERN_C PL_##v##_t* Perl_##p##v##_ptr(pTHX);
#define PERLVARI(p,v,t,i)	PERLVAR(p,v,t)
#define PERLVARIC(p,v,t,i) PERLVAR(p,v, const t)

#include "perlvars.h"

#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC

END_EXTERN_C

#if defined(PERL_CORE)

/* accessor functions for Perl "global" variables */

/* these need to be mentioned here, or most linkers won't put them in
   the perl executable */

#ifndef PERL_NO_FORCE_LINK

START_EXTERN_C

#ifndef DOINIT
EXTCONST void * const PL_force_link_funcs[];
#else
EXTCONST void * const PL_force_link_funcs[] = {
#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC
#define PERLVAR(p,v,t)		(void*)Perl_##p##v##_ptr,
#define PERLVARA(p,v,n,t)	PERLVAR(p,v,t)
#define PERLVARI(p,v,t,i)	PERLVAR(p,v,t)
#define PERLVARIC(p,v,t,i)	PERLVAR(p,v,t)

/* In Tru64 (__DEC && __osf__) the cc option -std1 causes that one
 * cannot cast between void pointers and function pointers without
 * info level warnings.  The PL_force_link_funcs[] would cause a few
 * hundred of those warnings.  In code one can circumnavigate this by using
 * unions that overlay the different pointers, but in declarations one
 * cannot use this trick.  Therefore we just disable the warning here
 * for the duration of the PL_force_link_funcs[] declaration. */

#if defined(__DECC) && defined(__osf__)
#pragma message save
#pragma message disable (nonstandcast)
#endif

#include "perlvars.h"

#if defined(__DECC) && defined(__osf__)
#pragma message restore
#endif

#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC
};
#endif	/* DOINIT */

END_EXTERN_C

#endif	/* PERL_NO_FORCE_LINK */

#else	/* !PERL_CORE */

EOT

foreach $sym (@globvar) {
    print $capih
	"#undef  PL_$sym\n" . hide("PL_$sym", "(*Perl_G${sym}_ptr(NULL))");
}

print $capih <<'EOT';

#endif /* !PERL_CORE */
#endif /* MULTIPLICITY && PERL_GLOBAL_STRUCT */

#endif /* __perlapi_h__ */
EOT

read_only_bottom_close_and_rename($capih);

my $capi = open_print_header('perlapi.c', <<'EOQ');
 *
 *
 * Up to the threshold of the door there mounted a flight of twenty-seven
 * broad stairs, hewn by some unknown art of the same black stone.  This
 * was the only entrance to the tower; ...
 *
 *     [p.577 of _The Lord of the Rings_, III/x: "The Voice of Saruman"]
 *
 */
EOQ

print $capi <<'EOT';
#include "EXTERN.h"
#include "perl.h"
#include "perlapi.h"

#if defined (MULTIPLICITY) && defined (PERL_GLOBAL_STRUCT)

/* accessor functions for Perl "global" variables */
START_EXTERN_C

#undef PERLVARI
#define PERLVARI(p,v,t,i) PERLVAR(p,v,t)

#undef PERLVAR
#undef PERLVARA
#define PERLVAR(p,v,t)		t* Perl_##p##v##_ptr(pTHX)		\
			{ dVAR; PERL_UNUSED_CONTEXT; return &(PL_##v); }
#define PERLVARA(p,v,n,t)	PL_##v##_t* Perl_##p##v##_ptr(pTHX)	\
			{ dVAR; PERL_UNUSED_CONTEXT; return &(PL_##v); }
#undef PERLVARIC
#define PERLVARIC(p,v,t,i)	\
			const t* Perl_##p##v##_ptr(pTHX)		\
			{ PERL_UNUSED_CONTEXT; return (const t *)&(PL_##v); }
#include "perlvars.h"

#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC

END_EXTERN_C

#endif /* MULTIPLICITY && PERL_GLOBAL_STRUCT */
EOT

read_only_bottom_close_and_rename($capi);

# ex: set ts=8 sts=4 sw=4 noet:
