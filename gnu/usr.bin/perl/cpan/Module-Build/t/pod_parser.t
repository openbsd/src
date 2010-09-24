#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest tests => 9;

blib_load('Module::Build::PodParser');

#########################

{
  package IO::StringBased;

  sub TIEHANDLE {
    my ($class, $string) = @_;
    return bless {
		  data => [ map "$_\n", split /\n/, $string],
		 }, $class;
  }

  sub READLINE {
    shift @{ shift()->{data} };
  }
}

local *FH;
tie *FH, 'IO::StringBased', <<'EOF';
=head1 NAME

Foo::Bar - Perl extension for blah blah blah

=head1 AUTHOR

C<Foo::Bar> was written by Engelbert Humperdinck I<E<lt>eh@example.comE<gt>> in 2004.

Home page: http://example.com/~eh/

=cut
EOF


my $pp = Module::Build::PodParser->new(fh => \*FH);
ok $pp, 'object created';

is $pp->get_author->[0], 'C<Foo::Bar> was written by Engelbert Humperdinck I<E<lt>eh@example.comE<gt>> in 2004.', 'author';
is $pp->get_abstract, 'Perl extension for blah blah blah', 'abstract';


{
  # Try again without a valid author spec
  untie *FH;
  tie *FH, 'IO::StringBased', <<'EOF';
=head1 NAME

Foo::Bar - Perl extension for blah blah blah

=cut
EOF

  my $pp = Module::Build::PodParser->new(fh => \*FH);
  ok $pp, 'object created';

  is_deeply $pp->get_author, [], 'author';
  is $pp->get_abstract, 'Perl extension for blah blah blah', 'abstract';
}


{
    # Try again with mixed-case =head1s.
  untie *FH;
  tie *FH, 'IO::StringBased', <<'EOF';
=head1 Name

Foo::Bar - Perl extension for blah blah blah

=head1 Author

C<Foo::Bar> was written by Engelbert Humperdinck I<E<lt>eh@example.comE<gt>> in 2004.

Home page: http://example.com/~eh/

=cut
EOF

  my $pp = Module::Build::PodParser->new(fh => \*FH);
  ok $pp, 'object created';

  is $pp->get_author->[0], 'C<Foo::Bar> was written by Engelbert Humperdinck I<E<lt>eh@example.comE<gt>> in 2004.', 'author';
  is $pp->get_abstract, 'Perl extension for blah blah blah', 'abstract';
}
