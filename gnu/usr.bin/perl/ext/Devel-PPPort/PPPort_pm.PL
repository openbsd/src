################################################################################
#
#  PPPort_pm.PL -- generate PPPort.pm
#
################################################################################
#
#  $Revision: 1.2 $
#  $Author: millert $
#  $Date: 2009/10/12 18:24:26 $
#
################################################################################
#
#  Version 3.x, Copyright (C) 2004-2009, Marcus Holland-Moritz.
#  Version 2.x, Copyright (C) 2001, Paul Marquess.
#  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

use strict;
$^W = 1;
require "parts/ppptools.pl";

my $INCLUDE = 'parts/inc';
my $DPPP = 'DPPP_';

my %embed = map { ( $_->{name} => $_ ) }
            parse_embed(qw(parts/embed.fnc parts/apidoc.fnc parts/ppport.fnc));

my(%provides, %prototypes, %explicit);

my $data = do { local $/; <DATA> };
$data =~ s{^\%(include)\s+(\w+)((?:[^\S\r\n]+.*?)?)\s*$}
          {eval "$1('$2', $3)" or die $@}gem;

$data = expand($data);

my @api = sort { lc $a cmp lc $b } keys %provides;

$data =~ s{^(.*)__PROVIDED_API__(\s*?)^}
          {join '', map "$1$_\n", @api}gem;

{
  my $len = 0;
  for (keys %explicit) {
    length > $len and $len = length;
  }
  my $format = sprintf '%%-%ds  %%-%ds  %%s', $len+2, $len+5;
  $len = 3*$len + 23;

$data =~ s!^(.*)__EXPLICIT_API__(\s*?)^!
           sprintf("$1$format\n", 'Function / Variable', 'Static Request', 'Global Request') .
           $1 . '-'x$len . "\n" .
           join('', map { sprintf "$1$format\n", $explicit{$_} eq 'var' ? $_ : "$_()", "NEED_$_", "NEED_${_}_GLOBAL" }
                    sort keys %explicit)
          !gem;
}

my %raw_base = %{&parse_todo('parts/base')};
my %raw_todo = %{&parse_todo('parts/todo')};

my %todo;
for (keys %raw_todo) {
  push @{$todo{$raw_todo{$_}}}, $_;
}

# check consistency
for (@api) {
  if (exists $raw_todo{$_} and exists $raw_base{$_}) {
    if ($raw_base{$_} eq $raw_todo{$_}) {
      warn "$INCLUDE/$provides{$_} provides $_, which is still marked "
           . "todo for " . format_version($raw_todo{$_}) . "\n";
    }
    else {
      check(2, "$_ was ported back to " . format_version($raw_todo{$_}) .
               " (baseline revision: " . format_version($raw_base{$_}) . ").");
    }
  }
}

my @perl_api;
for (keys %provides) {
  next if /^Perl_(.*)/ && exists $embed{$1};
  next if exists $embed{$_};
  push @perl_api, $_;
  check(2, "No API definition for provided element $_ found.");
}

push @perl_api, keys %embed;

for (@perl_api) {
  if (exists $provides{$_} && !exists $raw_base{$_}) {
    check(2, "Mmmh, $_ doesn't seem to need backporting.");
  }
  my $line = "$_|" . (exists $provides{$_} && exists $raw_base{$_} ? $raw_base{$_} : '') . '|';
  $line .= ($raw_todo{$_} || '') . '|';
  $line .= 'p' if exists $provides{$_};
  if (exists $embed{$_}) {
    my $e = $embed{$_};
    if (exists $e->{flags}{p}) {
      my $args = $e->{args};
      $line .= 'v' if @$args && $args->[-1][0] eq '...';
    }
    $line .= 'n' if exists $e->{flags}{n};
  }
  $_ = $line;
}

$data =~ s/^([\t ]*)__PERL_API__(\s*?)$/
           join "\n", map "$1$_", sort @perl_api
          /gem;

my @todo;
for (reverse sort keys %todo) {
  my $ver = format_version($_);
  my $todo = "=item perl $ver\n\n";
  for (sort @{$todo{$_}}) {
    $todo .= "  $_\n";
  }
  push @todo, $todo;
}

$data =~ s{^__UNSUPPORTED_API__(\s*?)^}
          {join "\n", @todo}gem;

$data =~ s{__MIN_PERL__}{5.003}g;
$data =~ s{__MAX_PERL__}{5.10.0}g;

open FH, ">PPPort.pm" or die "PPPort.pm: $!\n";
print FH $data;
close FH;

exit 0;

sub include
{
  my($file, $opt) = @_;

  print "including $file\n";

  my $data = parse_partspec("$INCLUDE/$file");

  for (@{$data->{provides}}) {
    if (exists $provides{$_}) {
      if ($provides{$_} ne $file) {
        warn "$file: $_ already provided by $provides{$_}\n";
      }
    }
    else {
      $provides{$_} = $file;
    }
  }

  for (keys %{$data->{prototypes}}) {
    $prototypes{$_} = $data->{prototypes}{$_};
    $data->{implementation} =~ s/^$_(?=\s*\()/$DPPP(my_$_)/mg;
  }

  my $out = $data->{implementation};

  if (exists $opt->{indent}) {
    $out =~ s/^/$opt->{indent}/gm;
  }

  return $out;
}

sub expand
{
  my $code = shift;
  $code =~ s{^(\s*#\s*(?:el)?if\s+)(.*)$}{$1.expand_pp_expressions($2)}gem;
  $code =~ s{^\s*
              __UNDEFINED__
              \s+
              (
                ( \w+ )
                (?: \( [^)]* \) )?
              )
              [^\r\n\S]*
              (
                (?:[^\r\n\\]|\\[^\r\n])*
                (?:
                  \\
                  (?:\r\n|[\r\n])
                  (?:[^\r\n\\]|\\[^\r\n])*
                )*
              )
            \s*$}
            {expand_undefined($2, $1, $3)}gemx;
  $code =~ s{^([^\S\r\n]*)__NEED_VAR__\s+(.*?)\s+(\w+)(?:\s*=\s*([^;]+?))?\s*;\s*$}
            {expand_need_var($1, $3, $2, $4)}gem;
  $code =~ s{^([^\S\r\n]*)__NEED_DUMMY_VAR__\s+(.*?)\s+(\w+)(?:\s*=\s*([^;]+?))?\s*;\s*$}
            {expand_need_dummy_var($1, $3, $2, $4)}gem;
  return $code;
}

sub expand_need_var
{
  my($indent, $var, $type, $init) = @_;

  $explicit{$var} = 'var';

  my $myvar = "$DPPP(my_$var)";
  $init = defined $init ? " = $init" : "";

  my $code = <<ENDCODE;
#if defined(NEED_$var)
static $type $myvar$init;
#elif defined(NEED_${var}_GLOBAL)
$type $myvar$init;
#else
extern $type $myvar;
#endif
#define $var $myvar
ENDCODE

  $code =~ s/^/$indent/mg;

  return $code;
}

sub expand_need_dummy_var
{
  my($indent, $var, $type, $init) = @_;

  $explicit{$var} = 'var';

  my $myvar = "$DPPP(dummy_$var)";
  $init = defined $init ? " = $init" : "";

  my $code = <<ENDCODE;
#if defined(NEED_$var)
static $type $myvar$init;
#elif defined(NEED_${var}_GLOBAL)
$type $myvar$init;
#else
extern $type $myvar;
#endif
ENDCODE

  $code =~ s/^/$indent/mg;

  return $code;
}

sub expand_undefined
{
  my($macro, $withargs, $def) = @_;
  my $rv = "#ifndef $macro\n#  define ";

  if (defined $def && $def =~ /\S/) {
    $rv .= sprintf "%-30s %s", $withargs, $def;
  }
  else {
    $rv .= $withargs;
  }

  $rv .= "\n#endif\n";

  return $rv;
}

sub expand_pp_expressions
{
  my $pp = shift;
  $pp =~ s/\{([^\}]+)\}/expand_pp_expr($1)/ge;
  return $pp;
}

sub expand_pp_expr
{
  my $expr = shift;

  if ($expr =~ /^\s*need\s+(\w+)\s*$/i) {
    my $func = $1;
    my $e = $embed{$func} or die "unknown API function '$func' in NEED\n";
    my $proto = make_prototype($e);
    if (exists $prototypes{$func}) {
      if (compare_prototypes($proto, $prototypes{$func})) {
        check(1, "differing prototypes for $func:\n  API: $proto\n  PPP: $prototypes{$func}");
        $proto = $prototypes{$func};
      }
    }
    else {
      warn "found no prototype for $func\n";;
    }

    $explicit{$func} = 'func';

    $proto =~ s/\b$func(?=\s*\()/$DPPP(my_$func)/;
    my $embed = make_embed($e);

    return "defined(NEED_$func)\n"
         . "static $proto;\n"
         . "static\n"
         . "#else\n"
         . "extern $proto;\n"
         . "#endif\n"
         . "\n"
         . "$embed\n"
         . "\n"
         . "#if defined(NEED_$func) || defined(NEED_${func}_GLOBAL)";
  }

  die "cannot expand preprocessor expression '$expr'\n";
}

sub make_embed
{
  my $f = shift;
  my $n = $f->{name};
  my $a = do { my $x = 'a'; join ',', map { $x++ } 1 .. @{$f->{args}} };
  my $lastarg = ${$f->{args}}[-1];

  if ($f->{flags}{n}) {
    if ($f->{flags}{p}) {
      return "#define $n $DPPP(my_$n)\n" .
             "#define Perl_$n $DPPP(my_$n)";
    }
    else {
      return "#define $n $DPPP(my_$n)";
    }
  }
  else {
    my $undef = <<UNDEF;
#ifdef $n
#  undef $n
#endif
UNDEF
    if ($f->{flags}{p}) {
      if ($f->{flags}{f}) {
        return "#define Perl_$n $DPPP(my_$n)";
      }
      elsif (@$lastarg && $lastarg->[0] =~ /\.\.\./) {
        return $undef . "#define $n $DPPP(my_$n)\n" .
                        "#define Perl_$n $DPPP(my_$n)";
      }
      else {
        return $undef . "#define $n($a) $DPPP(my_$n)(aTHX_ $a)\n" .
                        "#define Perl_$n $DPPP(my_$n)";
      }
    }
    else {
      return $undef . "#define $n($a) $DPPP(my_$n)(aTHX_ $a)";
    }
  }
}

sub check
{
  my $level = shift;

  if (exists $ENV{DPPP_CHECK_LEVEL} and $ENV{DPPP_CHECK_LEVEL} >= $level) {
    print STDERR @_, "\n";
  }
}

__DATA__
################################################################################
#
#  !!!!! Do NOT edit this file directly! -- Edit PPPort_pm.PL instead. !!!!!
#
#  This file was automatically generated from the definition files in the
#  parts/inc/ subdirectory by PPPort_pm.PL. To learn more about how all this
#  works, please read the F<HACKERS> file that came with this distribution.
#
################################################################################
#
#  Perl/Pollution/Portability
#
################################################################################
#
#  $Revision: 1.2 $
#  $Author: millert $
#  $Date: 2009/10/12 18:24:26 $
#
################################################################################
#
#  Version 3.x, Copyright (C) 2004-2009, Marcus Holland-Moritz.
#  Version 2.x, Copyright (C) 2001, Paul Marquess.
#  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

=head1 NAME

Devel::PPPort - Perl/Pollution/Portability

=head1 SYNOPSIS

    Devel::PPPort::WriteFile();   # defaults to ./ppport.h
    Devel::PPPort::WriteFile('someheader.h');

=head1 DESCRIPTION

Perl's API has changed over time, gaining new features, new functions,
increasing its flexibility, and reducing the impact on the C namespace
environment (reduced pollution). The header file written by this module,
typically F<ppport.h>, attempts to bring some of the newer Perl API
features to older versions of Perl, so that you can worry less about
keeping track of old releases, but users can still reap the benefit.

C<Devel::PPPort> contains a single function, called C<WriteFile>. Its
only purpose is to write the F<ppport.h> C header file. This file
contains a series of macros and, if explicitly requested, functions that
allow XS modules to be built using older versions of Perl. Currently,
Perl versions from __MIN_PERL__ to __MAX_PERL__ are supported.

This module is used by C<h2xs> to write the file F<ppport.h>.

=head2 Why use ppport.h?

You should use F<ppport.h> in modern code so that your code will work
with the widest range of Perl interpreters possible, without significant
additional work.

You should attempt older code to fully use F<ppport.h>, because the
reduced pollution of newer Perl versions is an important thing. It's so
important that the old polluting ways of original Perl modules will not be
supported very far into the future, and your module will almost certainly
break! By adapting to it now, you'll gain compatibility and a sense of
having done the electronic ecology some good.

=head2 How to use ppport.h

Don't direct the users of your module to download C<Devel::PPPort>.
They are most probably no XS writers. Also, don't make F<ppport.h>
optional. Rather, just take the most recent copy of F<ppport.h> that
you can find (e.g. by generating it with the latest C<Devel::PPPort>
release from CPAN), copy it into your project, adjust your project to
use it, and distribute the header along with your module.

=head2 Running ppport.h

But F<ppport.h> is more than just a C header. It's also a Perl script
that can check your source code. It will suggest hints and portability
notes, and can even make suggestions on how to change your code. You
can run it like any other Perl program:

    perl ppport.h [options] [files]

It also has embedded documentation, so you can use

    perldoc ppport.h

to find out more about how to use it.

=head1 FUNCTIONS

=head2 WriteFile

C<WriteFile> takes one optional argument. When called with one
argument, it expects to be passed a filename. When called with
no arguments, it defaults to the filename F<ppport.h>.

The function returns a true value if the file was written successfully.
Otherwise it returns a false value.

=head1 COMPATIBILITY

F<ppport.h> supports Perl versions from __MIN_PERL__ to __MAX_PERL__
in threaded and non-threaded configurations.

=head2 Provided Perl compatibility API

The header file written by this module, typically F<ppport.h>, provides
access to the following elements of the Perl API that is not available
in older Perl releases:

    __PROVIDED_API__

=head2 Perl API not supported by ppport.h

There is still a big part of the API not supported by F<ppport.h>.
Either because it doesn't make sense to back-port that part of the API,
or simply because it hasn't been implemented yet. Patches welcome!

Here's a list of the currently unsupported API, and also the version of
Perl below which it is unsupported:

=over 4

__UNSUPPORTED_API__

=back

=head1 BUGS

If you find any bugs, C<Devel::PPPort> doesn't seem to build on your
system or any of its tests fail, please use the CPAN Request Tracker
at L<http://rt.cpan.org/> to create a ticket for the module.

=head1 AUTHORS

=over 2

=item *

Version 1.x of Devel::PPPort was written by Kenneth Albanowski.

=item *

Version 2.x was ported to the Perl core by Paul Marquess.

=item *

Version 3.x was ported back to CPAN by Marcus Holland-Moritz.

=back

=head1 COPYRIGHT

Version 3.x, Copyright (C) 2004-2009, Marcus Holland-Moritz.

Version 2.x, Copyright (C) 2001, Paul Marquess.

Version 1.x, Copyright (C) 1999, Kenneth Albanowski.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1 SEE ALSO

See L<h2xs>, L<ppport.h>.

=cut

package Devel::PPPort;

use strict;
use vars qw($VERSION $data);

$VERSION = do { my @r = '$Snapshot: /Devel-PPPort/3.19 $' =~ /(\d+\.\d+(?:_\d+)?)/; @r ? $r[0] : '9.99' };

sub _init_data
{
  $data = do { local $/; <DATA> };
  my $pkg = 'Devel::PPPort';
  $data =~ s/__PERL_VERSION__/$]/g;
  $data =~ s/__VERSION__/$VERSION/g;
  $data =~ s/__PKG__/$pkg/g;
  $data =~ s/^\|>//gm;
}

sub WriteFile
{
  my $file = shift || 'ppport.h';
  defined $data or _init_data();
  my $copy = $data;
  $copy =~ s/\bppport\.h\b/$file/g;

  open F, ">$file" or return undef;
  print F $copy;
  close F;

  return 1;
}

1;

__DATA__
#if 0
<<'SKIP';
#endif
/*
----------------------------------------------------------------------

    ppport.h -- Perl/Pollution/Portability Version __VERSION__

    Automatically created by __PKG__ running under perl __PERL_VERSION__.

    Do NOT edit this file directly! -- Edit PPPort_pm.PL and the
    includes in parts/inc/ instead.

    Use 'perldoc ppport.h' to view the documentation below.

----------------------------------------------------------------------

SKIP

%include ppphdoc { indent => '|>' }

%include ppphbin

__DATA__
*/

#ifndef _P_P_PORTABILITY_H_
#define _P_P_PORTABILITY_H_

#ifndef DPPP_NAMESPACE
#  define DPPP_NAMESPACE DPPP_
#endif

#define DPPP_CAT2(x,y) CAT2(x,y)
#define DPPP_(name) DPPP_CAT2(DPPP_NAMESPACE, name)

%include version

%include threads

%include limits

%include uv

%include memory

%include misc

%include variables

%include mPUSH

%include call

%include newRV

%include newCONSTSUB

%include MY_CXT

%include format

%include SvREFCNT

%include newSV_type

%include newSVpv

%include SvPV

%include Sv_set

%include sv_xpvf

%include shared_pv

%include HvNAME

%include gv

%include warn

%include pvs

%include magic

%include cop

%include grok

%include snprintf

%include sprintf

%include exception

%include strlfuncs

%include pv_tools

#endif /* _P_P_PORTABILITY_H_ */

/* End of File ppport.h */
