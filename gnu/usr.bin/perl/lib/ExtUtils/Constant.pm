package ExtUtils::Constant;
use vars qw (@ISA $VERSION %XS_Constant %XS_TypeSet @EXPORT_OK %EXPORT_TAGS);
$VERSION = '0.12';

=head1 NAME

ExtUtils::Constant - generate XS code to import C header constants

=head1 SYNOPSIS

    use ExtUtils::Constant qw (WriteConstants);
    WriteConstants(
        NAME => 'Foo',
        NAMES => [qw(FOO BAR BAZ)],
    );
    # Generates wrapper code to make the values of the constants FOO BAR BAZ
    #  available to perl

=head1 DESCRIPTION

ExtUtils::Constant facilitates generating C and XS wrapper code to allow
perl modules to AUTOLOAD constants defined in C library header files.
It is principally used by the C<h2xs> utility, on which this code is based.
It doesn't contain the routines to scan header files to extract these
constants.

=head1 USAGE

Generally one only needs to call the C<WriteConstants> function, and then

    #include "const-c.inc"

in the C section of C<Foo.xs>

    INCLUDE const-xs.inc

in the XS section of C<Foo.xs>.

For greater flexibility use C<constant_types()>, C<C_constant> and
C<XS_constant>, with which C<WriteConstants> is implemented.

Currently this module understands the following types. h2xs may only know
a subset. The sizes of the numeric types are chosen by the C<Configure>
script at compile time.

=over 4

=item IV

signed integer, at least 32 bits.

=item UV

unsigned integer, the same size as I<IV>

=item NV

floating point type, probably C<double>, possibly C<long double>

=item PV

NUL terminated string, length will be determined with C<strlen>

=item PVN

A fixed length thing, given as a [pointer, length] pair. If you know the
length of a string at compile time you may use this instead of I<PV>

=item SV

A B<mortal> SV.

=item YES

Truth.  (C<PL_sv_yes>)  The value is not needed (and ignored).

=item NO

Defined Falsehood.  (C<PL_sv_no>)  The value is not needed (and ignored).

=item UNDEF

C<undef>.  The value of the macro is not needed.

=back

=head1 FUNCTIONS

=over 4

=cut

if ($] >= 5.006) {
  eval "use warnings; 1" or die $@;
}
use strict;
use Carp;

use Exporter;
use Text::Wrap;
$Text::Wrap::huge = 'overflow';
$Text::Wrap::columns = 80;

@ISA = 'Exporter';

%EXPORT_TAGS = ( 'all' => [ qw(
	XS_constant constant_types return_clause memEQ_clause C_stringify
	C_constant autoload WriteConstants WriteMakefileSnippet
) ] );

@EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

# '' is used as a flag to indicate non-ascii macro names, and hence the need
# to pass in the utf8 on/off flag.
%XS_Constant = (
		''    => '',
		IV    => 'PUSHi(iv)',
		UV    => 'PUSHu((UV)iv)',
		NV    => 'PUSHn(nv)',
		PV    => 'PUSHp(pv, strlen(pv))',
		PVN   => 'PUSHp(pv, iv)',
		SV    => 'PUSHs(sv)',
		YES   => 'PUSHs(&PL_sv_yes)',
		NO    => 'PUSHs(&PL_sv_no)',
		UNDEF => '',	# implicit undef
);

%XS_TypeSet = (
		IV    => '*iv_return =',
		UV    => '*iv_return = (IV)',
		NV    => '*nv_return =',
		PV    => '*pv_return =',
		PVN   => ['*pv_return =', '*iv_return = (IV)'],
		SV    => '*sv_return = ',
		YES   => undef,
		NO    => undef,
		UNDEF => undef,
);


=item C_stringify NAME

A function which returns a 7 bit ASCII correctly \ escaped version of the
string passed suitable for C's "" or ''. It will die if passed Unicode
characters.

=cut

# Hopefully make a happy C identifier.
sub C_stringify {
  local $_ = shift;
  return unless defined $_;
  confess "Wide character in '$_' intended as a C identifier" if tr/\0-\377//c;
  s/\\/\\\\/g;
  s/([\"\'])/\\$1/g;	# Grr. fix perl mode.
  s/\n/\\n/g;		# Ensure newlines don't end up in octal
  s/\r/\\r/g;
  s/\t/\\t/g;
  s/\f/\\f/g;
  s/\a/\\a/g;
  s/([^\0-\177])/sprintf "\\%03o", ord $1/ge;
  unless ($] < 5.006) {
    # This will elicit a warning on 5.005_03 about [: :] being reserved unless
    # I cheat
    my $cheat = '([[:^print:]])';
    s/$cheat/sprintf "\\%03o", ord $1/ge;
  } else {
    require POSIX;
    s/([^A-Za-z0-9_])/POSIX::isprint($1) ? $1 : sprintf "\\%03o", ord $1/ge;
  }
  $_;
}

=item perl_stringify NAME

A function which returns a 7 bit ASCII correctly \ escaped version of the
string passed suitable for a perl "" string.

=cut

# Hopefully make a happy perl identifier.
sub perl_stringify {
  local $_ = shift;
  return unless defined $_;
  s/\\/\\\\/g;
  s/([\"\'])/\\$1/g;	# Grr. fix perl mode.
  s/\n/\\n/g;		# Ensure newlines don't end up in octal
  s/\r/\\r/g;
  s/\t/\\t/g;
  s/\f/\\f/g;
  s/\a/\\a/g;
  s/([^\0-\177])/sprintf "\\x{%X}", ord $1/ge;
  unless ($] < 5.006) {
    # This will elicit a warning on 5.005_03 about [: :] being reserved unless
    # I cheat
    my $cheat = '([[:^print:]])';
    s/$cheat/sprintf "\\%03o", ord $1/ge;
  } else {
    require POSIX;
    s/([^A-Za-z0-9_])/POSIX::isprint($1) ? $1 : sprintf "\\%03o", ord $1/ge;
  }
  $_;
}

=item constant_types

A function returning a single scalar with C<#define> definitions for the
constants used internally between the generated C and XS functions.

=cut

sub constant_types () {
  my $start = 1;
  my @lines;
  push @lines, "#define PERL_constant_NOTFOUND\t$start\n"; $start++;
  push @lines, "#define PERL_constant_NOTDEF\t$start\n"; $start++;
  foreach (sort keys %XS_Constant) {
    next if $_ eq '';
    push @lines, "#define PERL_constant_IS$_\t$start\n"; $start++;
  }
  push @lines, << 'EOT';

#ifndef NVTYPE
typedef double NV; /* 5.6 and later define NVTYPE, and typedef NV to it.  */
#endif
#ifndef aTHX_
#define aTHX_ /* 5.6 or later define this for threading support.  */
#endif
#ifndef pTHX_
#define pTHX_ /* 5.6 or later define this for threading support.  */
#endif
EOT

  return join '', @lines;
}

=item memEQ_clause NAME, CHECKED_AT, INDENT

A function to return a suitable C C<if> statement to check whether I<NAME>
is equal to the C variable C<name>. If I<CHECKED_AT> is defined, then it
is used to avoid C<memEQ> for short names, or to generate a comment to
highlight the position of the character in the C<switch> statement.

=cut

sub memEQ_clause {
#    if (memEQ(name, "thingy", 6)) {
  # Which could actually be a character comparison or even ""
  my ($name, $checked_at, $indent) = @_;
  $indent = ' ' x ($indent || 4);
  my $len = length $name;

  if ($len < 2) {
    return $indent . "{\n" if (defined $checked_at and $checked_at == 0);
    # We didn't switch, drop through to the code for the 2 character string
    $checked_at = 1;
  }
  if ($len < 3 and defined $checked_at) {
    my $check;
    if ($checked_at == 1) {
      $check = 0;
    } elsif ($checked_at == 0) {
      $check = 1;
    }
    if (defined $check) {
      my $char = C_stringify (substr $name, $check, 1);
      return $indent . "if (name[$check] == '$char') {\n";
    }
  }
  # Could optimise a memEQ on 3 to 2 single character checks here
  $name = C_stringify ($name);
  my $body = $indent . "if (memEQ(name, \"$name\", $len)) {\n";
    $body .= $indent . "/*               ". (' ' x $checked_at) . '^'
      . (' ' x ($len - $checked_at + length $len)) . "    */\n"
        if defined $checked_at;
  return $body;
}

=item assign INDENT, TYPE, PRE, POST, VALUE...

A function to return a suitable assignment clause. If I<TYPE> is aggregate
(eg I<PVN> expects both pointer and length) then there should be multiple
I<VALUE>s for the components. I<PRE> and I<POST> if defined give snippets
of C code to proceed and follow the assignment. I<PRE> will be at the start
of a block, so variables may be defined in it.

=cut

# Hmm. value undef to to NOTDEF? value () to do NOTFOUND?

sub assign {
  my $indent = shift;
  my $type = shift;
  my $pre = shift;
  my $post = shift || '';
  my $clause;
  my $close;
  if ($pre) {
    chomp $pre;
    $clause = $indent . "{\n$pre";
    $clause .= ";" unless $pre =~ /;$/;
    $clause .= "\n";
    $close = "$indent}\n";
    $indent .= "  ";
  }
  confess "undef \$type" unless defined $type;
  confess "Can't generate code for type $type" unless exists $XS_TypeSet{$type};
  my $typeset = $XS_TypeSet{$type};
  if (ref $typeset) {
    die "Type $type is aggregate, but only single value given"
      if @_ == 1;
    foreach (0 .. $#$typeset) {
      $clause .= $indent . "$typeset->[$_] $_[$_];\n";
    }
  } elsif (defined $typeset) {
    die "Aggregate value given for type $type"
      if @_ > 1;
    $clause .= $indent . "$typeset $_[0];\n";
  }
  chomp $post;
  if (length $post) {
    $clause .= "$post";
    $clause .= ";" unless $post =~ /;$/;
    $clause .= "\n";
  }
  $clause .= "${indent}return PERL_constant_IS$type;\n";
  $clause .= $close if $close;
  return $clause;
}

=item return_clause

return_clause ITEM, INDENT

A function to return a suitable C<#ifdef> clause. I<ITEM> is a hashref
(as passed to C<C_constant> and C<match_clause>. I<INDENT> is the number
of spaces to indent, defaulting to 6.

=cut

sub return_clause ($$) {
##ifdef thingy
#      *iv_return = thingy;
#      return PERL_constant_ISIV;
##else
#      return PERL_constant_NOTDEF;
##endif
  my ($item, $indent) = @_;

  my ($name, $value, $macro, $default, $pre, $post, $def_pre, $def_post, $type)
    = @$item{qw (name value macro default pre post def_pre def_post type)};
  $value = $name unless defined $value;
  $macro = $name unless defined $macro;

  $macro = $value unless defined $macro;
  $indent = ' ' x ($indent || 6);
  unless ($type) {
    # use Data::Dumper; print STDERR Dumper ($item);
    confess "undef \$type";
  }

  my $clause;

  ##ifdef thingy
  if (ref $macro) {
    $clause = $macro->[0];
  } elsif ($macro ne "1") {
    $clause = "#ifdef $macro\n";
  }

  #      *iv_return = thingy;
  #      return PERL_constant_ISIV;
  $clause .= assign ($indent, $type, $pre, $post,
                     ref $value ? @$value : $value);

  if (ref $macro or $macro ne "1") {
    ##else
    $clause .= "#else\n";

    #      return PERL_constant_NOTDEF;
    if (!defined $default) {
      $clause .= "${indent}return PERL_constant_NOTDEF;\n";
    } else {
      my @default = ref $default ? @$default : $default;
      $type = shift @default;
      $clause .= assign ($indent, $type, $def_pre, $def_post, @default);
    }

    ##endif
    if (ref $macro) {
      $clause .= $macro->[1];
    } else {
      $clause .= "#endif\n";
    }
  }
  return $clause;
}

=pod

XXX document me

=cut

sub match_clause {
  # $offset defined if we have checked an offset.
  my ($item, $offset, $indent) = @_;
  $indent = ' ' x ($indent || 4);
  my $body = '';
  my ($no, $yes, $either, $name, $inner_indent);
  if (ref $item eq 'ARRAY') {
    ($yes, $no) = @$item;
    $either = $yes || $no;
    confess "$item is $either expecting hashref in [0] || [1]"
      unless ref $either eq 'HASH';
    $name = $either->{name};
  } else {
    confess "$item->{name} has utf8 flag '$item->{utf8}', should be false"
      if $item->{utf8};
    $name = $item->{name};
    $inner_indent = $indent;
  }

  $body .= memEQ_clause ($name, $offset, length $indent);
  if ($yes) {
    $body .= $indent . "  if (utf8) {\n";
  } elsif ($no) {
    $body .= $indent . "  if (!utf8) {\n";
  }
  if ($either) {
    $body .= return_clause ($either, 4 + length $indent);
    if ($yes and $no) {
      $body .= $indent . "  } else {\n";
      $body .= return_clause ($no, 4 + length $indent); 
    }
    $body .= $indent . "  }";
  } else {
    $body .= return_clause ($item, 2 + length $indent);
  }
  $body .= $indent . "}\n";
}

=item switch_clause INDENT, NAMELEN, ITEMHASH, ITEM...

An internal function to generate a suitable C<switch> clause, called by
C<C_constant> I<ITEM>s are in the hash ref format as given in the description
of C<C_constant>, and must all have the names of the same length, given by
I<NAMELEN> (This is not checked).  I<ITEMHASH> is a reference to a hash,
keyed by name, values being the hashrefs in the I<ITEM> list.
(No parameters are modified, and there can be keys in the I<ITEMHASH> that
are not in the list of I<ITEM>s without causing problems).

=cut

sub switch_clause {
  my ($indent, $comment, $namelen, $items, @items) = @_;
  $indent = ' ' x ($indent || 2);

  my @names = sort map {$_->{name}} @items;
  my $leader = $indent . '/* ';
  my $follower = ' ' x length $leader;
  my $body = $indent . "/* Names all of length $namelen.  */\n";
  if ($comment) {
    $body = wrap ($leader, $follower, $comment) . "\n";
    $leader = $follower;
  }
  my @safe_names = @names;
  foreach (@safe_names) {
    next unless tr/A-Za-z0-9_//c;
    $_ = '"' . perl_stringify ($_) . '"';
    # Ensure that the enclosing C comment doesn't end
    # by turning */  into *" . "/
    s!\*\/!\*"."/!gs;
    # gcc -Wall doesn't like finding /* inside a comment
    s!\/\*!/"."\*!gs;
  }
  $body .= wrap ($leader, $follower, join (" ", @safe_names) . " */") . "\n";
  # Figure out what to switch on.
  # (RMS, Spread of jump table, Position, Hashref)
  my @best = (1e38, ~0);
  foreach my $i (0 .. ($namelen - 1)) {
    my ($min, $max) = (~0, 0);
    my %spread;
    foreach (@names) {
      my $char = substr $_, $i, 1;
      my $ord = ord $char;
      $max = $ord if $ord > $max;
      $min = $ord if $ord < $min;
      push @{$spread{$char}}, $_;
      # warn "$_ $char";
    }
    # I'm going to pick the character to split on that minimises the root
    # mean square of the number of names in each case. Normally this should
    # be the one with the most keys, but it may pick a 7 where the 8 has
    # one long linear search. I'm not sure if RMS or just sum of squares is
    # actually better.
    # $max and $min are for the tie-breaker if the root mean squares match.
    # Assuming that the compiler may be building a jump table for the
    # switch() then try to minimise the size of that jump table.
    # Finally use < not <= so that if it still ties the earliest part of
    # the string wins. Because if that passes but the memEQ fails, it may
    # only need the start of the string to bin the choice.
    # I think. But I'm micro-optimising. :-)
    my $ss;
    $ss += @$_ * @$_ foreach values %spread;
    my $rms = sqrt ($ss / keys %spread);
    if ($rms < $best[0] || ($rms == $best[0] && ($max - $min) < $best[1])) {
      @best = ($rms, $max - $min, $i, \%spread);
    }
  }
  die "Internal error. Failed to pick a switch point for @names"
    unless defined $best[2];
  # use Data::Dumper; print Dumper (@best);
  my ($offset, $best) = @best[2,3];
  $body .= $indent . "/* Offset $offset gives the best switch position.  */\n";
  $body .= $indent . "switch (name[$offset]) {\n";
  foreach my $char (sort keys %$best) {
    $body .= $indent . "case '" . C_stringify ($char) . "':\n";
    foreach my $name (sort @{$best->{$char}}) {
      my $thisone = $items->{$name};
      # warn "You are here";
      $body .= match_clause ($thisone, $offset, 2 + length $indent);
    }
    $body .= $indent . "  break;\n";
  }
  $body .= $indent . "}\n";
  return $body;
}

=item params WHAT

An internal function. I<WHAT> should be a hashref of types the constant
function will return. I<params> returns a hashref keyed IV NV PV SV to show
which combination of pointers will be needed in the C argument list.

=cut

sub params {
  my $what = shift;
  foreach (sort keys %$what) {
    warn "ExtUtils::Constant doesn't know how to handle values of type $_" unless defined $XS_Constant{$_};
  }
  my $params = {};
  $params->{''} = 1 if $what->{''};
  $params->{IV} = 1 if $what->{IV} || $what->{UV} || $what->{PVN};
  $params->{NV} = 1 if $what->{NV};
  $params->{PV} = 1 if $what->{PV} || $what->{PVN};
  $params->{SV} = 1 if $what->{SV};
  return $params;
}

=item dump_names

dump_names DEFAULT_TYPE, TYPES, INDENT, OPTIONS, ITEM...

An internal function to generate the embedded perl code that will regenerate
the constant subroutines.  I<DEFAULT_TYPE>, I<TYPES> and I<ITEM>s are the
same as for C_constant.  I<INDENT> is treated as number of spaces to indent
by.  I<OPTIONS> is a hashref of options. Currently only C<declare_types> is
recognised.  If the value is true a C<$types> is always declared in the perl
code generated, if defined and false never declared, and if undefined C<$types>
is only declared if the values in I<TYPES> as passed in cannot be inferred from
I<DEFAULT_TYPES> and the I<ITEM>s.

=cut

sub dump_names {
  my ($default_type, $what, $indent, $options, @items) = @_;
  my $declare_types = $options->{declare_types};
  $indent = ' ' x ($indent || 0);

  my $result;
  my (@simple, @complex, %used_types);
  foreach (@items) {
    my $type;
    if (ref $_) {
      $type = $_->{type} || $default_type;
      if ($_->{utf8}) {
        # For simplicity always skip the bytes case, and reconstitute this entry
        # from its utf8 twin.
        next if $_->{utf8} eq 'no';
        # Copy the hashref, as we don't want to mess with the caller's hashref.
        $_ = {%$_};
        utf8::decode ($_->{name});
        delete $_->{utf8};
      }
    } else {
      $_ = {name=>$_};
      $type = $default_type;
    }
    $used_types{$type}++;
    if ($type eq $default_type and 0 == ($_->{name} =~ tr/A-Za-z0-9_//c)
        and !defined ($_->{macro}) and !defined ($_->{value})
        and !defined ($_->{default}) and !defined ($_->{pre})
        and !defined ($_->{post}) and !defined ($_->{def_pre})
        and !defined ($_->{def_post})) {
      # It's the default type, and the name consists only of A-Za-z0-9_
      push @simple, $_->{name};
    } else {
      push @complex, $_;
    }
  }

  if (!defined $declare_types) {
    # Do they pass in any types we weren't already using?
    foreach (keys %$what) {
      next if $used_types{$_};
      $declare_types++; # Found one in $what that wasn't used.
      last; # And one is enough to terminate this loop
    }
  }
  if ($declare_types) {
    $result = $indent . 'my $types = {map {($_, 1)} qw('
      . join (" ", sort keys %$what) . ")};\n";
  }
  $result .= wrap ($indent . "my \@names = (qw(",
		   $indent . "               ", join (" ", sort @simple) . ")");
  if (@complex) {
    foreach my $item (sort {$a->{name} cmp $b->{name}} @complex) {
      my $name = perl_stringify $item->{name};
      my $line = ",\n$indent            {name=>\"$name\"";
      $line .= ", type=>\"$item->{type}\"" if defined $item->{type};
      foreach my $thing (qw (macro value default pre post def_pre def_post)) {
        my $value = $item->{$thing};
        if (defined $value) {
          if (ref $value) {
            $line .= ", $thing=>[\""
              . join ('", "', map {perl_stringify $_} @$value) . '"]';
          } else {
            $line .= ", $thing=>\"" . perl_stringify($value) . "\"";
          }
        }
      }
      $line .= "}";
      # Ensure that the enclosing C comment doesn't end
      # by turning */  into *" . "/
      $line =~ s!\*\/!\*" . "/!gs;
      # gcc -Wall doesn't like finding /* inside a comment
      $line =~ s!\/\*!/" . "\*!gs;
      $result .= $line;
    }
  }
  $result .= ");\n";

  $result;
}


=item dogfood

dogfood PACKAGE, SUBNAME, DEFAULT_TYPE, TYPES, INDENT, BREAKOUT, ITEM...

An internal function to generate the embedded perl code that will regenerate
the constant subroutines.  Parameters are the same as for C_constant.

=cut

sub dogfood {
  my ($package, $subname, $default_type, $what, $indent, $breakout, @items)
    = @_;
  my $result = <<"EOT";
  /* When generated this function returned values for the list of names given
     in this section of perl code.  Rather than manually editing these functions
     to add or remove constants, which would result in this comment and section
     of code becoming inaccurate, we recommend that you edit this section of
     code, and use it to regenerate a new set of constant functions which you
     then use to replace the originals.

     Regenerate these constant functions by feeding this entire source file to
     perl -x

#!$^X -w
use ExtUtils::Constant qw (constant_types C_constant XS_constant);

EOT
  $result .= dump_names ($default_type, $what, 0, {declare_types=>1}, @items);
  $result .= <<'EOT';

print constant_types(); # macro defs
EOT
  $package = perl_stringify($package);
  $result .=
    "foreach (C_constant (\"$package\", '$subname', '$default_type', \$types, ";
  # The form of the indent parameter isn't defined. (Yet)
  if (defined $indent) {
    require Data::Dumper;
    $Data::Dumper::Terse=1;
    $Data::Dumper::Terse=1; # Not used once. :-)
    chomp ($indent = Data::Dumper::Dumper ($indent));
    $result .= $indent;
  } else {
    $result .= 'undef';
  }
  $result .= ", $breakout" . ', @names) ) {
    print $_, "\n"; # C constant subs
}
print "#### XS Section:\n";
print XS_constant ("' . $package . '", $types);
__END__
   */

';

  $result;
}

=item C_constant

C_constant PACKAGE, SUBNAME, DEFAULT_TYPE, TYPES, INDENT, BREAKOUT, ITEM...

A function that returns a B<list> of C subroutine definitions that return
the value and type of constants when passed the name by the XS wrapper.
I<ITEM...> gives a list of constant names. Each can either be a string,
which is taken as a C macro name, or a reference to a hash with the following
keys

=over 8

=item name

The name of the constant, as seen by the perl code.

=item type

The type of the constant (I<IV>, I<NV> etc)

=item value

A C expression for the value of the constant, or a list of C expressions if
the type is aggregate. This defaults to the I<name> if not given.

=item macro

The C pre-processor macro to use in the C<#ifdef>. This defaults to the
I<name>, and is mainly used if I<value> is an C<enum>. If a reference an
array is passed then the first element is used in place of the C<#ifdef>
line, and the second element in place of the C<#endif>. This allows
pre-processor constructions such as

    #if defined (foo)
    #if !defined (bar)
    ...
    #endif
    #endif

to be used to determine if a constant is to be defined.

A "macro" 1 signals that the constant is always defined, so the C<#if>/C<#endif>
test is omitted.

=item default

Default value to use (instead of C<croak>ing with "your vendor has not
defined...") to return if the macro isn't defined. Specify a reference to
an array with type followed by value(s).

=item pre

C code to use before the assignment of the value of the constant. This allows
you to use temporary variables to extract a value from part of a C<struct>
and return this as I<value>. This C code is places at the start of a block,
so you can declare variables in it.

=item post

C code to place between the assignment of value (to a temporary) and the
return from the function. This allows you to clear up anything in I<pre>.
Rarely needed.

=item def_pre
=item def_post

Equivalents of I<pre> and I<post> for the default value.

=item utf8

Generated internally. Is zero or undefined if name is 7 bit ASCII,
"no" if the name is 8 bit (and so should only match if SvUTF8() is false),
"yes" if the name is utf8 encoded.

The internals automatically clone any name with characters 128-255 but none
256+ (ie one that could be either in bytes or utf8) into a second entry
which is utf8 encoded.

=back

I<PACKAGE> is the name of the package, and is only used in comments inside the
generated C code.

The next 5 arguments can safely be given as C<undef>, and are mainly used
for recursion. I<SUBNAME> defaults to C<constant> if undefined.

I<DEFAULT_TYPE> is the type returned by C<ITEM>s that don't specify their
type. In turn it defaults to I<IV>. I<TYPES> should be given either as a comma
separated list of types that the C subroutine C<constant> will generate or as
a reference to a hash. I<DEFAULT_TYPE> will be added to the list if not
present, as will any types given in the list of I<ITEM>s. The resultant list
should be the same list of types that C<XS_constant> is given. [Otherwise
C<XS_constant> and C<C_constant> may differ in the number of parameters to the
constant function. I<INDENT> is currently unused and ignored. In future it may
be used to pass in information used to change the C indentation style used.]
The best way to maintain consistency is to pass in a hash reference and let
this function update it.

I<BREAKOUT> governs when child functions of I<SUBNAME> are generated.  If there
are I<BREAKOUT> or more I<ITEM>s with the same length of name, then the code
to switch between them is placed into a function named I<SUBNAME>_I<LEN>, for
example C<constant_5> for names 5 characters long.  The default I<BREAKOUT> is
3.  A single C<ITEM> is always inlined.

=cut

# The parameter now BREAKOUT was previously documented as:
#
# I<NAMELEN> if defined signals that all the I<name>s of the I<ITEM>s are of
# this length, and that the constant name passed in by perl is checked and
# also of this length. It is used during recursion, and should be C<undef>
# unless the caller has checked all the lengths during code generation, and
# the generated subroutine is only to be called with a name of this length.
#
# As you can see it now performs this function during recursion by being a
# scalar reference.

sub C_constant {
  my ($package, $subname, $default_type, $what, $indent, $breakout, @items)
    = @_;
  $package ||= 'Foo';
  $subname ||= 'constant';
  # I'm not using this. But a hashref could be used for full formatting without
  # breaking this API
  # $indent ||= 0;

  my ($namelen, $items);
  if (ref $breakout) {
    # We are called recursively. We trust @items to be normalised, $what to
    # be a hashref, and pinch %$items from our parent to save recalculation.
    ($namelen, $items) = @$breakout;
  } else {
    $breakout ||= 3;
    $default_type ||= 'IV';
    if (!ref $what) {
      # Convert line of the form IV,UV,NV to hash
      $what = {map {$_ => 1} split /,\s*/, ($what || '')};
      # Figure out what types we're dealing with, and assign all unknowns to the
      # default type
    }
    my @new_items;
    foreach my $orig (@items) {
      my ($name, $item);
      if (ref $orig) {
        # Make a copy which is a normalised version of the ref passed in.
        $name = $orig->{name};
        my ($type, $macro, $value) = @$orig{qw (type macro value)};
        $type ||= $default_type;
        $what->{$type} = 1;
        $item = {name=>$name, type=>$type};

        undef $macro if defined $macro and $macro eq $name;
        $item->{macro} = $macro if defined $macro;
        undef $value if defined $value and $value eq $name;
        $item->{value} = $value if defined $value;
        foreach my $key (qw(default pre post def_pre def_post)) {
          my $value = $orig->{$key};
          $item->{$key} = $value if defined $value;
          # warn "$key $value";
        }
      } else {
        $name = $orig;
        $item = {name=>$name, type=>$default_type};
        $what->{$default_type} = 1;
      }
      warn "ExtUtils::Constant doesn't know how to handle values of type $_ used in macro $name" unless defined $XS_Constant{$item->{type}};
      if ($name !~ tr/\0-\177//c) {
        # No characters outside 7 bit ASCII.
        if (exists $items->{$name}) {
          die "Multiple definitions for macro $name";
        }
        $items->{$name} = $item;
      } else {
        # No characters outside 8 bit. This is hardest.
        if (exists $items->{$name} and ref $items->{$name} ne 'ARRAY') {
          confess "Unexpected ASCII definition for macro $name";
        }
        if ($name !~ tr/\0-\377//c) {
          $item->{utf8} = 'no';
          $items->{$name}[1] = $item;
          push @new_items, $item;
          # Copy item, to create the utf8 variant.
          $item = {%$item};
        }
        # Encode the name as utf8 bytes.
        utf8::encode($name);
        if ($items->{$name}[0]) {
          die "Multiple definitions for macro $name";
        }
        $item->{utf8} = 'yes';
        $item->{name} = $name;
        $items->{$name}[0] = $item;
        # We have need for the utf8 flag.
        $what->{''} = 1;
      }
      push @new_items, $item;
    }
    @items = @new_items;
    # use Data::Dumper; print Dumper @items;
  }
  my $params = params ($what);

  my ($body, @subs) = "static int\n$subname (pTHX_ const char *name";
  $body .= ", STRLEN len" unless defined $namelen;
  $body .= ", int utf8" if $params->{''};
  $body .= ", IV *iv_return" if $params->{IV};
  $body .= ", NV *nv_return" if $params->{NV};
  $body .= ", const char **pv_return" if $params->{PV};
  $body .= ", SV **sv_return" if $params->{SV};
  $body .= ") {\n";

  if (defined $namelen) {
    # We are a child subroutine. Print the simple description
    my $comment = 'When generated this function returned values for the list'
      . ' of names given here.  However, subsequent manual editing may have'
        . ' added or removed some.';
    $body .= switch_clause (2, $comment, $namelen, $items, @items);
  } else {
    # We are the top level.
    $body .= "  /* Initially switch on the length of the name.  */\n";
    $body .= dogfood ($package, $subname, $default_type, $what, $indent,
                      $breakout, @items);
    $body .= "  switch (len) {\n";
    # Need to group names of the same length
    my @by_length;
    foreach (@items) {
      push @{$by_length[length $_->{name}]}, $_;
    }
    foreach my $i (0 .. $#by_length) {
      next unless $by_length[$i];	# None of this length
      $body .= "  case $i:\n";
      if (@{$by_length[$i]} == 1) {
        $body .= match_clause ($by_length[$i]->[0]);
      } elsif (@{$by_length[$i]} < $breakout) {
        $body .= switch_clause (4, '', $i, $items, @{$by_length[$i]});
      } else {
        # Only use the minimal set of parameters actually needed by the types
        # of the names of this length.
        my $what = {};
        foreach (@{$by_length[$i]}) {
          $what->{$_->{type}} = 1;
          $what->{''} = 1 if $_->{utf8};
        }
        $params = params ($what);
        push @subs, C_constant ($package, "${subname}_$i", $default_type, $what,
                                $indent, [$i, $items], @{$by_length[$i]});
        $body .= "    return ${subname}_$i (aTHX_ name";
        $body .= ", utf8" if $params->{''};
        $body .= ", iv_return" if $params->{IV};
        $body .= ", nv_return" if $params->{NV};
        $body .= ", pv_return" if $params->{PV};
        $body .= ", sv_return" if $params->{SV};
        $body .= ");\n";
      }
      $body .= "    break;\n";
    }
    $body .= "  }\n";
  }
  $body .= "  return PERL_constant_NOTFOUND;\n}\n";
  return (@subs, $body);
}

=item XS_constant PACKAGE, TYPES, SUBNAME, C_SUBNAME

A function to generate the XS code to implement the perl subroutine
I<PACKAGE>::constant used by I<PACKAGE>::AUTOLOAD to load constants.
This XS code is a wrapper around a C subroutine usually generated by
C<C_constant>, and usually named C<constant>.

I<TYPES> should be given either as a comma separated list of types that the
C subroutine C<constant> will generate or as a reference to a hash. It should
be the same list of types as C<C_constant> was given.
[Otherwise C<XS_constant> and C<C_constant> may have different ideas about
the number of parameters passed to the C function C<constant>]

You can call the perl visible subroutine something other than C<constant> if
you give the parameter I<SUBNAME>. The C subroutine it calls defaults to
the name of the perl visible subroutine, unless you give the parameter
I<C_SUBNAME>.

=cut

sub XS_constant {
  my $package = shift;
  my $what = shift;
  my $subname = shift;
  my $C_subname = shift;
  $subname ||= 'constant';
  $C_subname ||= $subname;

  if (!ref $what) {
    # Convert line of the form IV,UV,NV to hash
    $what = {map {$_ => 1} split /,\s*/, ($what)};
  }
  my $params = params ($what);
  my $type;

  my $xs = <<"EOT";
void
$subname(sv)
    PREINIT:
#ifdef dXSTARG
	dXSTARG; /* Faster if we have it.  */
#else
	dTARGET;
#endif
	STRLEN		len;
        int		type;
EOT

  if ($params->{IV}) {
    $xs .= "	IV		iv;\n";
  } else {
    $xs .= "	/* IV\t\tiv;\tUncomment this if you need to return IVs */\n";
  }
  if ($params->{NV}) {
    $xs .= "	NV		nv;\n";
  } else {
    $xs .= "	/* NV\t\tnv;\tUncomment this if you need to return NVs */\n";
  }
  if ($params->{PV}) {
    $xs .= "	const char	*pv;\n";
  } else {
    $xs .=
      "	/* const char\t*pv;\tUncomment this if you need to return PVs */\n";
  }

  $xs .= << 'EOT';
    INPUT:
	SV *		sv;
        const char *	s = SvPV(sv, len);
EOT
  if ($params->{''}) {
  $xs .= << 'EOT';
    INPUT:
	int		utf8 = SvUTF8(sv);
EOT
  }
  $xs .= << 'EOT';
    PPCODE:
EOT

  if ($params->{IV} xor $params->{NV}) {
    $xs .= << "EOT";
        /* Change this to $C_subname(aTHX_ s, len, &iv, &nv);
           if you need to return both NVs and IVs */
EOT
  }
  $xs .= "	type = $C_subname(aTHX_ s, len";
  $xs .= ', utf8' if $params->{''};
  $xs .= ', &iv' if $params->{IV};
  $xs .= ', &nv' if $params->{NV};
  $xs .= ', &pv' if $params->{PV};
  $xs .= ', &sv' if $params->{SV};
  $xs .= ");\n";

  $xs .= << "EOT";
      /* Return 1 or 2 items. First is error message, or undef if no error.
           Second, if present, is found value */
        switch (type) {
        case PERL_constant_NOTFOUND:
          sv = sv_2mortal(newSVpvf("%s is not a valid $package macro", s));
          PUSHs(sv);
          break;
        case PERL_constant_NOTDEF:
          sv = sv_2mortal(newSVpvf(
	    "Your vendor has not defined $package macro %s, used", s));
          PUSHs(sv);
          break;
EOT

  foreach $type (sort keys %XS_Constant) {
    # '' marks utf8 flag needed.
    next if $type eq '';
    $xs .= "\t/* Uncomment this if you need to return ${type}s\n"
      unless $what->{$type};
    $xs .= "        case PERL_constant_IS$type:\n";
    if (length $XS_Constant{$type}) {
      $xs .= << "EOT";
          EXTEND(SP, 1);
          PUSHs(&PL_sv_undef);
          $XS_Constant{$type};
EOT
    } else {
      # Do nothing. return (), which will be correctly interpreted as
      # (undef, undef)
    }
    $xs .= "          break;\n";
    unless ($what->{$type}) {
      chop $xs; # Yes, another need for chop not chomp.
      $xs .= " */\n";
    }
  }
  $xs .= << "EOT";
        default:
          sv = sv_2mortal(newSVpvf(
	    "Unexpected return type %d while processing $package macro %s, used",
               type, s));
          PUSHs(sv);
        }
EOT

  return $xs;
}


=item autoload PACKAGE, VERSION, AUTOLOADER

A function to generate the AUTOLOAD subroutine for the module I<PACKAGE>
I<VERSION> is the perl version the code should be backwards compatible with.
It defaults to the version of perl running the subroutine.  If I<AUTOLOADER>
is true, the AUTOLOAD subroutine falls back on AutoLoader::AUTOLOAD for all
names that the constant() routine doesn't recognise.

=cut

# ' # Grr. syntax highlighters that don't grok pod.

sub autoload {
  my ($module, $compat_version, $autoloader) = @_;
  $compat_version ||= $];
  croak "Can't maintain compatibility back as far as version $compat_version"
    if $compat_version < 5;
  my $func = "sub AUTOLOAD {\n"
  . "    # This AUTOLOAD is used to 'autoload' constants from the constant()\n"
  . "    # XS function.";
  $func .= "  If a constant is not found then control is passed\n"
  . "    # to the AUTOLOAD in AutoLoader." if $autoloader;


  $func .= "\n\n"
  . "    my \$constname;\n";
  $func .=
    "    our \$AUTOLOAD;\n"  if ($compat_version >= 5.006);

  $func .= <<"EOT";
    (\$constname = \$AUTOLOAD) =~ s/.*:://;
    croak "&${module}::constant not defined" if \$constname eq 'constant';
    my (\$error, \$val) = constant(\$constname);
EOT

  if ($autoloader) {
    $func .= <<'EOT';
    if ($error) {
	if ($error =~  /is not a valid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	} else {
	    croak $error;
	}
    }
EOT
  } else {
    $func .=
      "    if (\$error) { croak \$error; }\n";
  }

  $func .= <<'END';
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
#XXX	if ($] >= 5.00561) {
#XXX	    *$AUTOLOAD = sub () { $val };
#XXX	}
#XXX	else {
	    *$AUTOLOAD = sub { $val };
#XXX	}
    }
    goto &$AUTOLOAD;
}

END

  return $func;
}


=item WriteMakefileSnippet

WriteMakefileSnippet ATTRIBUTE =E<gt> VALUE [, ...] 

A function to generate perl code for Makefile.PL that will regenerate
the constant subroutines.  Parameters are named as passed to C<WriteConstants>,
with the addition of C<INDENT> to specify the number of leading spaces
(default 2).

Currently only C<INDENT>, C<NAME>, C<DEFAULT_TYPE>, C<NAMES>, C<C_FILE> and
C<XS_FILE> are recognised.

=cut

sub WriteMakefileSnippet {
  my %args = @_;
  my $indent = $args{INDENT} || 2;

  my $result = <<"EOT";
ExtUtils::Constant::WriteConstants(
                                   NAME         => '$args{NAME}',
                                   NAMES        => \\\@names,
                                   DEFAULT_TYPE => '$args{DEFAULT_TYPE}',
EOT
  foreach (qw (C_FILE XS_FILE)) {
    next unless exists $args{$_};
    $result .= sprintf "                                   %-12s => '%s',\n",
      $_, $args{$_};
  }
  $result .= <<'EOT';
                                );
EOT

  $result =~ s/^/' 'x$indent/gem;
  return dump_names ($args{DEFAULT_TYPE}, undef, $indent, undef,
                           @{$args{NAMES}})
          . $result;
}

=item WriteConstants ATTRIBUTE =E<gt> VALUE [, ...]

Writes a file of C code and a file of XS code which you should C<#include>
and C<INCLUDE> in the C and XS sections respectively of your module's XS
code.  You probaby want to do this in your C<Makefile.PL>, so that you can
easily edit the list of constants without touching the rest of your module.
The attributes supported are

=over 4

=item NAME

Name of the module.  This must be specified

=item DEFAULT_TYPE

The default type for the constants.  If not specified C<IV> is assumed.

=item BREAKOUT_AT

The names of the constants are grouped by length.  Generate child subroutines
for each group with this number or more names in.

=item NAMES

An array of constants' names, either scalars containing names, or hashrefs
as detailed in L<"C_constant">.

=item C_FILE

The name of the file to write containing the C code.  The default is
C<const-c.inc>.  The C<-> in the name ensures that the file can't be
mistaken for anything related to a legitimate perl package name, and
not naming the file C<.c> avoids having to override Makefile.PL's
C<.xs> to C<.c> rules.

=item XS_FILE

The name of the file to write containing the XS code.  The default is
C<const-xs.inc>.

=item SUBNAME

The perl visible name of the XS subroutine generated which will return the
constants. The default is C<constant>.

=item C_SUBNAME

The name of the C subroutine generated which will return the constants.
The default is I<SUBNAME>.  Child subroutines have C<_> and the name
length appended, so constants with 10 character names would be in
C<constant_10> with the default I<XS_SUBNAME>.

=back

=cut

sub WriteConstants {
  my %ARGS =
    ( # defaults
     C_FILE =>       'const-c.inc',
     XS_FILE =>      'const-xs.inc',
     SUBNAME =>      'constant',
     DEFAULT_TYPE => 'IV',
     @_);

  $ARGS{C_SUBNAME} ||= $ARGS{SUBNAME}; # No-one sane will have C_SUBNAME eq '0'

  croak "Module name not specified" unless length $ARGS{NAME};

  open my $c_fh, ">$ARGS{C_FILE}" or die "Can't open $ARGS{C_FILE}: $!";
  open my $xs_fh, ">$ARGS{XS_FILE}" or die "Can't open $ARGS{XS_FILE}: $!";

  # As this subroutine is intended to make code that isn't edited, there's no
  # need for the user to specify any types that aren't found in the list of
  # names.
  my $types = {};

  print $c_fh constant_types(); # macro defs
  print $c_fh "\n";

  # indent is still undef. Until anyone implents indent style rules with it.
  foreach (C_constant ($ARGS{NAME}, $ARGS{C_SUBNAME}, $ARGS{DEFAULT_TYPE},
                       $types, undef, $ARGS{BREAKOUT_AT}, @{$ARGS{NAMES}})) {
    print $c_fh $_, "\n"; # C constant subs
  }
  print $xs_fh XS_constant ($ARGS{NAME}, $types, $ARGS{XS_SUBNAME},
                            $ARGS{C_SUBNAME});

  close $c_fh or warn "Error closing $ARGS{C_FILE}: $!";
  close $xs_fh or warn "Error closing $ARGS{XS_FILE}: $!";
}

1;
__END__

=back

=head1 AUTHOR

Nicholas Clark <nick@ccl4.org> based on the code in C<h2xs> by Larry Wall and
others

=cut
