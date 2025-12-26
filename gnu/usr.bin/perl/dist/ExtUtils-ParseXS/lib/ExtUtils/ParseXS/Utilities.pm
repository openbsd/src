package ExtUtils::ParseXS::Utilities;
use strict;
use warnings;
use Exporter;
use File::Spec;
use ExtUtils::ParseXS::Constants ();

our $VERSION = '3.57';

our (@ISA, @EXPORT_OK);
@ISA = qw(Exporter);
@EXPORT_OK = qw(
  standard_typemap_locations
  trim_whitespace
  C_string
  valid_proto_string
  process_typemaps
  map_type
  standard_XS_defs
  analyze_preprocessor_statement
  set_cond
  Warn
  WarnHint
  current_line_number
  blurt
  death
  check_conditional_preprocessor_statements
  escape_file_for_line_directive
  report_typemap_failure
);

=head1 NAME

ExtUtils::ParseXS::Utilities - Subroutines used with ExtUtils::ParseXS

=head1 SYNOPSIS

  use ExtUtils::ParseXS::Utilities qw(
    standard_typemap_locations
    trim_whitespace
    C_string
    valid_proto_string
    process_typemaps
    map_type
    standard_XS_defs
    analyze_preprocessor_statement
    set_cond
    Warn
    blurt
    death
    check_conditional_preprocessor_statements
    escape_file_for_line_directive
    report_typemap_failure
  );

=head1 SUBROUTINES

The following functions are not considered to be part of the public interface.
They are documented here for the benefit of future maintainers of this module.

=head2 C<standard_typemap_locations()>

=over 4

=item * Purpose

Provide a list of filepaths where F<typemap> files may be found.  The
filepaths -- relative paths to files (not just directory paths) -- appear in this list in lowest-to-highest priority.

The highest priority is to look in the current directory.  

  'typemap'

The second and third highest priorities are to look in the parent of the
current directory and a directory called F<lib/ExtUtils> underneath the parent
directory.

  '../typemap',
  '../lib/ExtUtils/typemap',

The fourth through ninth highest priorities are to look in the corresponding
grandparent, great-grandparent and great-great-grandparent directories.

  '../../typemap',
  '../../lib/ExtUtils/typemap',
  '../../../typemap',
  '../../../lib/ExtUtils/typemap',
  '../../../../typemap',
  '../../../../lib/ExtUtils/typemap',

The tenth and subsequent priorities are to look in directories named
F<ExtUtils> which are subdirectories of directories found in C<@INC> --
I<provided> a file named F<typemap> actually exists in such a directory.
Example:

  '/usr/local/lib/perl5/5.10.1/ExtUtils/typemap',

However, these filepaths appear in the list returned by
C<standard_typemap_locations()> in reverse order, I<i.e.>, lowest-to-highest.

  '/usr/local/lib/perl5/5.10.1/ExtUtils/typemap',
  '../../../../lib/ExtUtils/typemap',
  '../../../../typemap',
  '../../../lib/ExtUtils/typemap',
  '../../../typemap',
  '../../lib/ExtUtils/typemap',
  '../../typemap',
  '../lib/ExtUtils/typemap',
  '../typemap',
  'typemap'

=item * Arguments

  my @stl = standard_typemap_locations( \@INC );

Reference to C<@INC>.

=item * Return Value

Array holding list of directories to be searched for F<typemap> files.

=back

=cut

SCOPE: {
  my @tm_template;

  sub standard_typemap_locations {
    my $include_ref = shift;

    if (not @tm_template) {
      @tm_template = qw(typemap);

      my $updir = File::Spec->updir();
      foreach my $dir (
          File::Spec->catdir(($updir) x 1),
          File::Spec->catdir(($updir) x 2),
          File::Spec->catdir(($updir) x 3),
          File::Spec->catdir(($updir) x 4),
      ) {
        unshift @tm_template, File::Spec->catfile($dir, 'typemap');
        unshift @tm_template, File::Spec->catfile($dir, lib => ExtUtils => 'typemap');
      }
    }

    my @tm = @tm_template;
    foreach my $dir (@{ $include_ref}) {
      my $file = File::Spec->catfile($dir, ExtUtils => 'typemap');
      unshift @tm, $file if -e $file;
    }
    return @tm;
  }
} # end SCOPE

=head2 C<trim_whitespace()>

=over 4

=item * Purpose

Perform an in-place trimming of leading and trailing whitespace from the
first argument provided to the function.

=item * Argument

  trim_whitespace($arg);

=item * Return Value

None.  Remember:  this is an I<in-place> modification of the argument.

=back

=cut

sub trim_whitespace {
  $_[0] =~ s/^\s+|\s+$//go;
}

=head2 C<C_string()>

=over 4

=item * Purpose

Escape backslashes (C<\>) in prototype strings.

=item * Arguments

      $ProtoThisXSUB = C_string($_);

String needing escaping.

=item * Return Value

Properly escaped string.

=back

=cut

sub C_string {
  my($string) = @_;

  $string =~ s[\\][\\\\]g;
  $string;
}

=head2 C<valid_proto_string()>

=over 4

=item * Purpose

Validate prototype string.

=item * Arguments

String needing checking.

=item * Return Value

Upon success, returns the same string passed as argument.

Upon failure, returns C<0>.

=back

=cut

sub valid_proto_string {
  my ($string) = @_;

  if ( $string =~ /^$ExtUtils::ParseXS::Constants::PrototypeRegexp+$/ ) {
    return $string;
  }

  return 0;
}

=head2 C<process_typemaps()>

=over 4

=item * Purpose

Process all typemap files.

=item * Arguments

  my $typemaps_object = process_typemaps( $args{typemap}, $pwd );

List of two elements:  C<typemap> element from C<%args>; current working
directory.

=item * Return Value

Upon success, returns an L<ExtUtils::Typemaps> object.

=back

=cut

sub process_typemaps {
  my ($tmap, $pwd) = @_;

  my @tm = ref $tmap ? @{$tmap} : ($tmap);

  foreach my $typemap (@tm) {
    die "Can't find $typemap in $pwd\n" unless -r $typemap;
  }

  push @tm, standard_typemap_locations( \@INC );

  require ExtUtils::Typemaps;
  my $typemap = ExtUtils::Typemaps->new;
  foreach my $typemap_loc (@tm) {
    next unless -f $typemap_loc;
    # skip directories, binary files etc.
    warn("Warning: ignoring non-text typemap file '$typemap_loc'\n"), next
      unless -T $typemap_loc;

    $typemap->merge(file => $typemap_loc, replace => 1);
  }

  return $typemap;
}


=head2 C<map_type($self, $type, $varname)>

Returns a mapped version of the C type C<$type>. In particular, it
converts C<Foo::bar> to C<Foo__bar>, converts the special C<array(type,n)>
into C<type *>, and inserts C<$varname> (if present) into any function
pointer type. So C<...(*)...> becomes C<...(* foo)...>.

=cut

sub map_type {
  my ExtUtils::ParseXS $self = shift;
  my ($type, $varname) = @_;

  # C++ has :: in types too so skip this
  $type =~ tr/:/_/ unless $self->{config_RetainCplusplusHierarchicalTypes};

  # map the special return type 'array(type, n)' to 'type *'
  $type =~ s/^array\(([^,]*),(.*)\).*/$1 */s;

  if ($varname) {
    if ($type =~ / \( \s* \* (?= \s* \) ) /xg) {
      (substr $type, pos $type, 0) = " $varname ";
    }
    else {
      $type .= "\t$varname";
    }
  }
  return $type;
}


=head2 C<standard_XS_defs()>

=over 4

=item * Purpose

Writes to the C<.c> output file certain preprocessor directives and function
headers needed in all such files.

=item * Arguments

None.

=item * Return Value

Returns true.

=back

=cut

sub standard_XS_defs {
  print <<"EOF";
#ifndef PERL_UNUSED_VAR
#  define PERL_UNUSED_VAR(var) if (0) var = var
#endif

#ifndef dVAR
#  define dVAR		dNOOP
#endif


/* This stuff is not part of the API! You have been warned. */
#ifndef PERL_VERSION_DECIMAL
#  define PERL_VERSION_DECIMAL(r,v,s) (r*1000000 + v*1000 + s)
#endif
#ifndef PERL_DECIMAL_VERSION
#  define PERL_DECIMAL_VERSION \\
	  PERL_VERSION_DECIMAL(PERL_REVISION,PERL_VERSION,PERL_SUBVERSION)
#endif
#ifndef PERL_VERSION_GE
#  define PERL_VERSION_GE(r,v,s) \\
	  (PERL_DECIMAL_VERSION >= PERL_VERSION_DECIMAL(r,v,s))
#endif
#ifndef PERL_VERSION_LE
#  define PERL_VERSION_LE(r,v,s) \\
	  (PERL_DECIMAL_VERSION <= PERL_VERSION_DECIMAL(r,v,s))
#endif

/* XS_INTERNAL is the explicit static-linkage variant of the default
 * XS macro.
 *
 * XS_EXTERNAL is the same as XS_INTERNAL except it does not include
 * "STATIC", ie. it exports XSUB symbols. You probably don't want that
 * for anything but the BOOT XSUB.
 *
 * See XSUB.h in core!
 */


/* TODO: This might be compatible further back than 5.10.0. */
#if PERL_VERSION_GE(5, 10, 0) && PERL_VERSION_LE(5, 15, 1)
#  undef XS_EXTERNAL
#  undef XS_INTERNAL
#  if defined(__CYGWIN__) && defined(USE_DYNAMIC_LOADING)
#    define XS_EXTERNAL(name) __declspec(dllexport) XSPROTO(name)
#    define XS_INTERNAL(name) STATIC XSPROTO(name)
#  endif
#  if defined(__SYMBIAN32__)
#    define XS_EXTERNAL(name) EXPORT_C XSPROTO(name)
#    define XS_INTERNAL(name) EXPORT_C STATIC XSPROTO(name)
#  endif
#  ifndef XS_EXTERNAL
#    if defined(HASATTRIBUTE_UNUSED) && !defined(__cplusplus)
#      define XS_EXTERNAL(name) void name(pTHX_ CV* cv __attribute__unused__)
#      define XS_INTERNAL(name) STATIC void name(pTHX_ CV* cv __attribute__unused__)
#    else
#      ifdef __cplusplus
#        define XS_EXTERNAL(name) extern "C" XSPROTO(name)
#        define XS_INTERNAL(name) static XSPROTO(name)
#      else
#        define XS_EXTERNAL(name) XSPROTO(name)
#        define XS_INTERNAL(name) STATIC XSPROTO(name)
#      endif
#    endif
#  endif
#endif

/* perl >= 5.10.0 && perl <= 5.15.1 */


/* The XS_EXTERNAL macro is used for functions that must not be static
 * like the boot XSUB of a module. If perl didn't have an XS_EXTERNAL
 * macro defined, the best we can do is assume XS is the same.
 * Dito for XS_INTERNAL.
 */
#ifndef XS_EXTERNAL
#  define XS_EXTERNAL(name) XS(name)
#endif
#ifndef XS_INTERNAL
#  define XS_INTERNAL(name) XS(name)
#endif

/* Now, finally, after all this mess, we want an ExtUtils::ParseXS
 * internal macro that we're free to redefine for varying linkage due
 * to the EXPORT_XSUB_SYMBOLS XS keyword. This is internal, use
 * XS_EXTERNAL(name) or XS_INTERNAL(name) in your code if you need to!
 */

#undef XS_EUPXS
#if defined(PERL_EUPXS_ALWAYS_EXPORT)
#  define XS_EUPXS(name) XS_EXTERNAL(name)
#else
   /* default to internal */
#  define XS_EUPXS(name) XS_INTERNAL(name)
#endif

EOF

  print <<"EOF";
#ifndef PERL_ARGS_ASSERT_CROAK_XS_USAGE
#define PERL_ARGS_ASSERT_CROAK_XS_USAGE assert(cv); assert(params)

/* prototype to pass -Wmissing-prototypes */
STATIC void
S_croak_xs_usage(const CV *const cv, const char *const params);

STATIC void
S_croak_xs_usage(const CV *const cv, const char *const params)
{
    const GV *const gv = CvGV(cv);

    PERL_ARGS_ASSERT_CROAK_XS_USAGE;

    if (gv) {
        const char *const gvname = GvNAME(gv);
        const HV *const stash = GvSTASH(gv);
        const char *const hvname = stash ? HvNAME(stash) : NULL;

        if (hvname)
	    Perl_croak_nocontext("Usage: %s::%s(%s)", hvname, gvname, params);
        else
	    Perl_croak_nocontext("Usage: %s(%s)", gvname, params);
    } else {
        /* Pants. I don't think that it should be possible to get here. */
	Perl_croak_nocontext("Usage: CODE(0x%" UVxf ")(%s)", PTR2UV(cv), params);
    }
}
#undef  PERL_ARGS_ASSERT_CROAK_XS_USAGE

#define croak_xs_usage        S_croak_xs_usage

#endif

/* NOTE: the prototype of newXSproto() is different in versions of perls,
 * so we define a portable version of newXSproto()
 */
#ifdef newXS_flags
#define newXSproto_portable(name, c_impl, file, proto) newXS_flags(name, c_impl, file, proto, 0)
#else
#define newXSproto_portable(name, c_impl, file, proto) (PL_Sv=(SV*)newXS(name, c_impl, file), sv_setpv(PL_Sv, proto), (CV*)PL_Sv)
#endif /* !defined(newXS_flags) */

#if PERL_VERSION_LE(5, 21, 5)
#  define newXS_deffile(a,b) Perl_newXS(aTHX_ a,b,file)
#else
#  define newXS_deffile(a,b) Perl_newXS_deffile(aTHX_ a,b)
#endif

/* simple backcompat versions of the TARGx() macros with no optimisation */
#ifndef TARGi
#  define TARGi(iv, do_taint) sv_setiv_mg(TARG, iv)
#  define TARGu(uv, do_taint) sv_setuv_mg(TARG, uv)
#  define TARGn(nv, do_taint) sv_setnv_mg(TARG, nv)
#endif

EOF
  return 1;
}

=head2 C<analyze_preprocessor_statement()>

=over 4

=item * Purpose

Process a CPP conditional line (C<#if> etc), to keep track of conditional
nesting. In particular, it updates C<< @{$self->{XS_parse_stack}} >> which
contains the current list of nested conditions, and
C<< $self->{XS_parse_stack_top_if_idx} >> which indicates the most recent
C<if> in that stack. So an C<#if> pushes, an C<#endif> pops, an C<#else>
modifies etc. Each element is a hash of the form:

  {
    type      => 'if',
    varname   => 'XSubPPtmpAAAA', # maintained by caller

                  # XS functions defined within this branch of the
                  # conditional (maintained by caller)
    functions =>  {
                    'Foo::Bar::baz' => 1,
                    ...
                  }
                  # XS functions seen within any previous branch
    other_functions => {... }

It also updates C<< $self->{bootcode_early} >> and
C<< $self->{bootcode_late} >> with extra CPP directives.

=item * Arguments

      $self->analyze_preprocessor_statement($statement);

=back

=cut

sub analyze_preprocessor_statement {
  my ExtUtils::ParseXS $self = shift;
  my ($statement) = @_;

  my $ix = $self->{XS_parse_stack_top_if_idx};

  if ($statement eq 'if') {
    # #if or #ifdef
    $ix = @{ $self->{XS_parse_stack} };
    push(@{ $self->{XS_parse_stack} }, {type => 'if'});
  }
  else {
    # An #else/#elsif/#endif.

    $self->death("Error: '$statement' with no matching 'if'")
      if $self->{XS_parse_stack}->[-1]{type} ne 'if';

    if ($self->{XS_parse_stack}->[-1]{varname}) {
      # close any '#ifdef XSubPPtmpAAAA' inserted earlier into boot code.
      push(@{ $self->{bootcode_early} }, "#endif\n");
      push(@{ $self->{bootcode_later} }, "#endif\n");
    }

    my(@fns) = keys %{$self->{XS_parse_stack}->[-1]{functions}};

    if ($statement ne 'endif') {
      # Add current functions to the hash of functions seen in previous
      # branch limbs, then reset for this next limb of the branch.
      @{$self->{XS_parse_stack}->[-1]{other_functions}}{@fns} = (1) x @fns;
      @{$self->{XS_parse_stack}->[-1]}{qw(varname functions)} = ('', {});
    }
    else {
      # #endif - pop stack and update new top entry
      my($tmp) = pop(@{ $self->{XS_parse_stack} });
      0 while (--$ix
           && $self->{XS_parse_stack}->[$ix]{type} ne 'if');

      # For all functions declared within any limb of the just-popped
      # if/endif, mark them as having appeared within this limb of the
      # outer nested branch.
      push(@fns, keys %{$tmp->{other_functions}});
      @{$self->{XS_parse_stack}->[$ix]{functions}}{@fns}  =  (1) x @fns;
    }
  }

  $self->{XS_parse_stack_top_if_idx} = $ix;
}


=head2 C<set_cond()>

=over 4

=item * Purpose

Return a string containing a snippet of C code which tests for the 'wrong
number of arguments passed' condition, depending on whether there are
default arguments or ellipsis.

=item * Arguments

C<ellipsis> true if the xsub's signature has a trailing C<, ...>.

C<$min_args> the smallest number of args which may be passed.

C<$num_args> the number of parameters in the signature.

=item * Return Value

The text of a short C code snippet.

=back

=cut

sub set_cond {
  my ($ellipsis, $min_args, $num_args) = @_;
  my $cond;
  if ($ellipsis) {
    $cond = ($min_args ? qq(items < $min_args) : 0);
  }
  elsif ($min_args == $num_args) {
    $cond = qq(items != $min_args);
  }
  else {
    $cond = qq(items < $min_args || items > $num_args);
  }
  return $cond;
}

=head2 C<current_line_number()>

=over 4

=item * Purpose

Figures out the current line number in the XS file.

=item * Arguments

C<$self>

=item * Return Value

The current line number.

=back

=cut

sub current_line_number {
  my ExtUtils::ParseXS $self = shift;
  my $line_number = $self->{line_no}->[@{ $self->{line_no} } - @{ $self->{line} } -1];
  return $line_number;
}



=head2 Error handling methods

There are four main methods for reporting warnings and errors.

=over

=item C<< $self->Warn(@messages) >>

This is equivalent to:

  warn "@messages in foo.xs, line 123\n";

The file and line number are based on the file currently being parsed. It
is intended for use where you wish to warn, but can continue parsing and
still generate a correct C output file.

=item C<< $self->blurt(@messages) >>

This is equivalent to C<Warn>, except that it also increments the internal
error count (which can be retrieved with C<report_error_count()>). It is
used to report an error, but where parsing can continue (so typically for
a semantic error rather than a syntax error). It is expected that the
caller will eventually signal failure in some fashion. For example,
C<xsubpp> has this as its last line:

  exit($self->report_error_count() ? 1 : 0);

=item C<< $self->death(@messages) >>

This normally equivalent to:

  $self->Warn(@messages);
  exit(1);

It is used for something like a syntax error, where parsing can't
continue.  However, this is inconvenient for testing purposes, as the
error can't be trapped. So if C<$self> is created with the C<die_on_error>
flag, or if C<$ExtUtils::ParseXS::DIE_ON_ERROR> is true when process_file()
is called, then instead it will die() with that message.

=item C<< $self->WarnHint(@messages, $hints) >>

This is a more obscure twin to C<Warn>, which does the same as C<Warn>,
but afterwards, outputs any lines contained in the C<$hints> string, with
each line wrapped in parentheses. For example:

  $self->WarnHint(@messages,
    "Have you set the foo switch?\nSee the manual for further info");

=back

=cut


# see L</Error handling methods> above

sub Warn {
  my ExtUtils::ParseXS $self = shift;
  $self->WarnHint(@_,undef);
}


# see L</Error handling methods> above

sub WarnHint {
  warn _MsgHint(@_);
}


# see L</Error handling methods> above

sub _MsgHint {
  my ExtUtils::ParseXS $self = shift;
  my $hint = pop;
  my $warn_line_number = $self->current_line_number();
  my $ret = join("",@_) . " in $self->{in_filename}, line $warn_line_number\n";
  if ($hint) {
    $ret .= "    ($_)\n" for split /\n/, $hint;
  }
  return $ret;
}


# see L</Error handling methods> above

sub blurt {
  my ExtUtils::ParseXS $self = shift;
  $self->Warn(@_);
  $self->{error_count}++
}


# see L</Error handling methods> above

sub death {
  my ExtUtils::ParseXS $self = $_[0];
  my $message = _MsgHint(@_,"");
  if ($self->{config_die_on_error}) {
    die $message;
  } else {
    warn $message;
  }
  exit 1;
}


=head2 C<check_conditional_preprocessor_statements()>

=over 4

=item * Purpose

Warn if the lines in C<< @{ $self->{line} } >> don't have balanced C<#if>,
C<endif> etc.

=item * Arguments

None

=item * Return Value

None

=back

=cut

sub check_conditional_preprocessor_statements {
  my ExtUtils::ParseXS $self = $_[0];
  my @cpp = grep(/^\#\s*(?:if|e\w+)/, @{ $self->{line} });
  if (@cpp) {
    my $cpplevel;
    for my $cpp (@cpp) {
      if ($cpp =~ /^\#\s*if/) {
        $cpplevel++;
      }
      elsif (!$cpplevel) {
        $self->Warn("Warning: #else/elif/endif without #if in this function");
        print STDERR "    (precede it with a blank line if the matching #if is outside the function)\n"
          if $self->{XS_parse_stack}->[-1]{type} eq 'if';
        return;
      }
      elsif ($cpp =~ /^\#\s*endif/) {
        $cpplevel--;
      }
    }
    $self->Warn("Warning: #if without #endif in this function") if $cpplevel;
  }
}

=head2 C<escape_file_for_line_directive()>

=over 4

=item * Purpose

Escapes a given code source name (typically a file name but can also
be a command that was read from) so that double-quotes and backslashes are escaped.

=item * Arguments

A string.

=item * Return Value

A string with escapes for double-quotes and backslashes.

=back

=cut

sub escape_file_for_line_directive {
  my $string = shift;
  $string =~ s/\\/\\\\/g;
  $string =~ s/"/\\"/g;
  return $string;
}

=head2 C<report_typemap_failure>

=over 4

=item * Purpose

Do error reporting for missing typemaps.

=item * Arguments

The C<ExtUtils::ParseXS> object.

An C<ExtUtils::Typemaps> object.

The string that represents the C type that was not found in the typemap.

Optionally, the string C<death> or C<blurt> to choose
whether the error is immediately fatal or not. Default: C<blurt>

=item * Return Value

Returns nothing. Depending on the arguments, this
may call C<death> or C<blurt>, the former of which is
fatal.

=back

=cut

sub report_typemap_failure {
  my ExtUtils::ParseXS $self = shift;
  my ($tm, $ctype, $error_method) = @_;
  $error_method ||= 'blurt';

  my @avail_ctypes = $tm->list_mapped_ctypes;

  my $err = "Could not find a typemap for C type '$ctype'.\n"
            . "The following C types are mapped by the current typemap:\n'"
            . join("', '", @avail_ctypes) . "'\n";

  $self->$error_method($err);
  return();
}


1;

# vim: ts=2 sw=2 et:
