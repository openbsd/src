# Scalar::Util.pm
#
# Copyright (c) 1997-2001 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package Scalar::Util;

require Exporter;
require List::Util; # List::Util loads the XS

our @ISA       = qw(Exporter);
our @EXPORT_OK = qw(blessed dualvar reftype weaken isweak tainted readonly openhandle);
our $VERSION   = $List::Util::VERSION;

sub openhandle ($) {
  my $fh = shift;
  my $rt = reftype($fh) || '';

  return defined(fileno($fh)) ? $fh : undef
    if $rt eq 'IO';

  if (reftype(\$fh) eq 'GLOB') { # handle  openhandle(*DATA)
    $fh = \(my $tmp=$fh);
  }
  elsif ($rt ne 'GLOB') {
    return undef;
  }

  (tied(*$fh) or defined(fileno($fh)))
    ? $fh : undef;
}

1;

__END__

=head1 NAME

Scalar::Util - A selection of general-utility scalar subroutines

=head1 SYNOPSIS

    use Scalar::Util qw(blessed dualvar isweak readonly reftype tainted weaken);

=head1 DESCRIPTION

C<Scalar::Util> contains a selection of subroutines that people have
expressed would be nice to have in the perl core, but the usage would
not really be high enough to warrant the use of a keyword, and the size
so small such that being individual extensions would be wasteful.

By default C<Scalar::Util> does not export any subroutines. The
subroutines defined are

=over 4

=item blessed EXPR

If EXPR evaluates to a blessed reference the name of the package
that it is blessed into is returned. Otherwise C<undef> is returned.

   $scalar = "foo";
   $class  = blessed $scalar;           # undef

   $ref    = [];
   $class  = blessed $ref;              # undef

   $obj    = bless [], "Foo";
   $class  = blessed $obj;              # "Foo"

=item dualvar NUM, STRING

Returns a scalar that has the value NUM in a numeric context and the
value STRING in a string context.

    $foo = dualvar 10, "Hello";
    $num = $foo + 2;                    # 12
    $str = $foo . " world";             # Hello world

=item isweak EXPR

If EXPR is a scalar which is a weak reference the result is true.

    $ref  = \$foo;
    $weak = isweak($ref);               # false
    weaken($ref);
    $weak = isweak($ref);               # true

=item openhandle FH

Returns FH if FH may be used as a filehandle and is open, or FH is a tied
handle. Otherwise C<undef> is returned.

    $fh = openhandle(*STDIN);		# \*STDIN
    $fh = openhandle(\*STDIN);		# \*STDIN
    $fh = openhandle(*NOTOPEN);		# undef
    $fh = openhandle("scalar");		# undef
    
=item readonly SCALAR

Returns true if SCALAR is readonly.

    sub foo { readonly($_[0]) }

    $readonly = foo($bar);              # false
    $readonly = foo(0);                 # true

=item reftype EXPR

If EXPR evaluates to a reference the type of the variable referenced
is returned. Otherwise C<undef> is returned.

    $type = reftype "string";           # undef
    $type = reftype \$var;              # SCALAR
    $type = reftype [];                 # ARRAY

    $obj  = bless {}, "Foo";
    $type = reftype $obj;               # HASH

=item tainted EXPR

Return true if the result of EXPR is tainted

    $taint = tainted("constant");       # false
    $taint = tainted($ENV{PWD});        # true if running under -T

=item weaken REF

REF will be turned into a weak reference. This means that it will not
hold a reference count on the object it references. Also when the reference
count on that object reaches zero, REF will be set to undef.

This is useful for keeping copies of references , but you don't want to
prevent the object being DESTROY-ed at its usual time.

    {
      my $var;
      $ref = \$var;
      weaken($ref);                     # Make $ref a weak reference
    }
    # $ref is now undef

=back

=head1 KNOWN BUGS

There is a bug in perl5.6.0 with UV's that are >= 1<<31. This will
show up as tests 8 and 9 of dualvar.t failing

=head1 COPYRIGHT

Copyright (c) 1997-2001 Graham Barr <gbarr@pobox.com>. All rights reserved.
This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

Except weaken and isweak which are

Copyright (c) 1999 Tuomas J. Lukka <lukka@iki.fi>. All rights reserved.
This program is free software; you can redistribute it and/or modify it
under the same terms as perl itself.

=head1 BLATANT PLUG

The weaken and isweak subroutines in this module and the patch to the core Perl
were written in connection  with the APress book `Tuomas J. Lukka's Definitive
Guide to Object-Oriented Programming in Perl', to avoid explaining why certain
things would have to be done in cumbersome ways.

=cut
