#!/usr/bin/perl -w
#
# Regenerate (overwriting only if changed):
#
#    embed.h
#    embedvar.h
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
    require './regen/regen_lib.pl';
    require './regen/embed_lib.pl';
}

my $unflagged_pointers;

#
# See database of global and static function prototypes in embed.fnc
# This is used to generate prototype headers under various configurations,
# export symbols lists for different platforms, and macros to provide an
# implicit interpreter context argument.
#

my $error_count = 0;
sub die_at_end ($) { # Keeps going for now, but makes sure the regen doesn't
                     # succeed.
    warn shift;
    $error_count++;
}

sub full_name ($$) { # Returns the function name with potentially the
                     # prefixes 'S_' or 'Perl_'
    my ($func, $flags) = @_;

    return "Perl_$func" if $flags =~ /p/;
    return "S_$func" if $flags =~ /[SIi]/;
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
        if ($flags =~ / ( [^AabCDdEefFGhIiMmNnOoPpRrSsTUuWXx] ) /x) {
            die_at_end "flag $1 is not legal (for function $plain_func)";
        }
        my @nonnull;
        my $args_assert_line = ( $flags !~ /G/ );
        my $has_depth = ( $flags =~ /W/ );
        my $has_context = ( $flags !~ /T/ );
        my $never_returns = ( $flags =~ /r/ );
        my $binarycompat = ( $flags =~ /b/ );
        my $commented_out = ( $flags =~ /m/ );
        my $is_malloc = ( $flags =~ /a/ );
        my $can_ignore = ( $flags !~ /R/ ) && ( $flags !~ /P/ ) && !$is_malloc;
        my @names_of_nn;
        my $func;

        if (! $can_ignore && $retval eq 'void') {
            warn "It is nonsensical to require the return value of a void function ($plain_func) to be checked";
        }

        die_at_end "$plain_func: S and p flags are mutually exclusive"
                                            if $flags =~ /S/ && $flags =~ /p/;
        die_at_end "$plain_func: m and $1 flags are mutually exclusive"
                                        if $flags =~ /m/ && $flags =~ /([pS])/;

        die_at_end "$plain_func: u flag only usable with m" if $flags =~ /u/
                                                            && $flags !~ /m/;

        my $static_inline = 0;
        if ($flags =~ /([SIi])/) {
            my $type;
            if ($never_returns) {
                $type = {
                    'S' => 'PERL_STATIC_NO_RET',
                    'i' => 'PERL_STATIC_INLINE_NO_RET',
                    'I' => 'PERL_STATIC_FORCE_INLINE_NO_RET'
                }->{$1};
            }
            else {
                $type = {
                    'S' => 'STATIC',
                    'i' => 'PERL_STATIC_INLINE',
                    'I' => 'PERL_STATIC_FORCE_INLINE'
                }->{$1};
            }
            $retval = "$type $retval";
            die_at_end "Don't declare static function '$plain_func' pure" if $flags =~ /P/;
            $static_inline = $type =~ /^PERL_STATIC(?:_FORCE)?_INLINE/;
        }
        else {
            if ($never_returns) {
                $retval = "PERL_CALLCONV_NO_RET $retval";
            }
            else {
                $retval = "PERL_CALLCONV $retval";
            }
        }

        $func = full_name($plain_func, $flags);

        die_at_end "For '$plain_func', M flag requires p flag"
                                            if $flags =~ /M/ && $flags !~ /p/;
        die_at_end "For '$plain_func', C flag requires one of [pIimb] flags"
						   if $flags =~ /C/
						   && ($flags !~ /[Iibmp]/

						      # Notwithstanding the
						      # above, if the name
						      # won't clash with a
						      # user name, it's ok.
						   && $plain_func !~ /^[Pp]erl/);

        die_at_end "For '$plain_func', X flag requires one of [Iip] flags"
                                            if $flags =~ /X/ && $flags !~ /[Iip]/;
        die_at_end "For '$plain_func', X and m flags are mutually exclusive"
                                            if $flags =~ /X/ && $flags =~ /m/;
        die_at_end "For '$plain_func', [Ii] with [ACX] requires p flag"
                        if $flags =~ /[Ii]/ && $flags =~ /[ACX]/ && $flags !~ /p/;
        die_at_end "For '$plain_func', b and m flags are mutually exclusive"
                 . " (try M flag)" if $flags =~ /b/ && $flags =~ /m/;
        die_at_end "For '$plain_func', b flag without M flag requires D flag"
                            if $flags =~ /b/ && $flags !~ /M/ && $flags !~ /D/;
        die_at_end "For '$plain_func', I and i flags are mutually exclusive"
                                            if $flags =~ /I/ && $flags =~ /i/;

        $ret = "";
        $ret .= "$retval\t$func(";
        if ( $has_context ) {
            $ret .= @args ? "pTHX_ " : "pTHX";
        }
        if (@args) {
            die_at_end "n flag is contradicted by having arguments"
                                                                if $flags =~ /n/;
            my $n;
            for my $arg ( @args ) {
                ++$n;
                if (   $args_assert_line
		    && $arg =~ /\*/
		    && $arg !~ /\b(NN|NULLOK)\b/ )
		{
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
                    die_at_end "$func: $arg ($n) doesn't have a name\n";
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
        $ret .= " _pDEPTH" if $has_depth;
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
        if ( $flags =~ /I/ ) {
            push @attrs, "__attribute__always_inline__";
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
        elsif ((grep { $_ eq '...' } @args) && $flags !~ /F/) {
            die_at_end "$plain_func: Function with '...' arguments must have"
                     . " f or F flag";
        }
        if ( @attrs ) {
            $ret .= "\n";
            $ret .= join( "\n", map { "\t\t\t$_" } @attrs );
        }
        $ret .= ";";
        $ret = "/* $ret */" if $commented_out;

        $ret .= "\n#define PERL_ARGS_ASSERT_\U$plain_func\E"
                                            if $args_assert_line || @names_of_nn;
        $ret .= "\t\\\n\t" . join '; ', map "assert($_)", @names_of_nn
                                                                if @names_of_nn;

        $ret = "#ifndef PERL_NO_INLINE_FUNCTIONS\n$ret\n#endif" if $static_inline;
        $ret = "#ifndef NO_MATHOMS\n$ret\n#endif" if $binarycompat;
        $ret .= @attrs ? "\n\n" : "\n";

        print $pr $ret;
    }

    print $pr <<'EOF';
#ifdef PERL_CORE
#  include "pp_proto.h"
#endif
END_EXTERN_C
EOF

    read_only_bottom_close_and_rename($pr) if ! $error_count;
}

die_at_end "$unflagged_pointers pointer arguments to clean up\n" if $unflagged_pointers;

sub readvars {
    my ($file, $pre) = @_;
    local (*FILE, $_);
    my %seen;
    open(FILE, '<', $file)
        or die "embed.pl: Can't open $file: $!\n";
    while (<FILE>) {
        s/[ \t]*#.*//;		# Delete comments.
        if (/PERLVARA?I?C?\($pre,\s*(\w+)/) {
            die_at_end "duplicate symbol $1 while processing $file line $.\n"
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
 * Not defining the short forms is a good thing for cleaner embedding.
 * BEWARE that a bunch of macros don't have long names, so either must be
 * added or don't use them if you define this symbol */

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
        unless ($flags =~ /[omM]/) {
            my $args = scalar @args;
            if ($flags =~ /T/) {
                my $full_name = full_name($func, $flags);
                next if $full_name eq $func;	# Don't output a no-op.
                $ret = hide($func, $full_name);
            }
            elsif ($args and $args[$args-1] =~ /\.\.\./) {
                if ($flags =~ /p/) {
                    # we're out of luck for varargs functions under CPP
                    # So we can only do these macros for non-MULTIPLICITY perls:
                    $ret = "#ifndef MULTIPLICITY\n"
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
                $ret .= $alist;
                if ($flags =~ /W/) {
                    if ($alist) {
                        $ret .= " _aDEPTH";
                    } else {
                        die "Can't use W without other args (currently)";
                    }
                }
                $ret .= ")\n";
            }
            $ret = "#ifndef NO_MATHOMS\n$ret#endif\n" if $flags =~ /b/;
        }
        $lines .= $ret;
    }
    # Prune empty #if/#endif pairs.
    while ($lines =~ s/#\s*if[^\n]+\n#\s*endif\n//) {
    }
    # Merge adjacent blocks.
    while ($lines =~ s/(#ifndef MULTIPLICITY
[^\n]+
)#endif
#ifndef MULTIPLICITY
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
#if defined(MULTIPLICITY) && !defined(PERL_NO_SHORT_NAMES)
END

foreach (@nocontext) {
    print $em hide($_, "Perl_${_}_nocontext", "  ");
}

print $em <<'END';
#endif

#endif /* !defined(PERL_CORE) && !defined(PERL_NOCOMPAT) */

#if !defined(MULTIPLICITY)
/* undefined symbols, point them back at the usual ones */
END

foreach (@nocontext) {
    print $em hide("Perl_${_}_nocontext", "Perl_$_", "  ");
}

print $em <<'END';
#endif
END

read_only_bottom_close_and_rename($em) if ! $error_count;

$em = open_print_header('embedvar.h');

print $em <<'END';
#if defined(MULTIPLICITY)
#  define vTHX	aTHX
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
END

read_only_bottom_close_and_rename($em) if ! $error_count;

die "$error_count errors found" if $error_count;

# ex: set ts=8 sts=4 sw=4 noet:
