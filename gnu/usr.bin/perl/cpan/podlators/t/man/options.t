#!/usr/bin/perl -w
#
# Additional tests for Pod::Man options.
#
# Copyright 2002, 2004, 2006, 2008, 2009, 2012, 2013, 2015
#     Russ Allbery <rra@cpan.org>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use 5.006;
use strict;
use warnings;

use lib 't/lib';

use Test::More tests => 31;
use Test::Podlators qw(read_test_data slurp);

BEGIN {
    use_ok ('Pod::Man');
}

# Redirect stderr to a file.  Return the name of the file that stores standard
# error.
sub stderr_save {
    open(OLDERR, '>&STDERR') or die "Can't dup STDERR: $!\n";
    open(STDERR, "> out$$.err") or die "Can't redirect STDERR: $!\n";
    return "out$$.err";
}

# Restore stderr.
sub stderr_restore {
    close(STDERR);
    open(STDERR, '>&OLDERR') or die "Can't dup STDERR: $!\n";
    close(OLDERR);
}

# Loop through all the test data, generate output, and compare it to the
# desired output data.
my %options = (options => 1, errors => 1);
my $n = 1;
while (defined(my $data_ref = read_test_data(\*DATA, \%options))) {
    my $parser = Pod::Man->new(%{ $data_ref->{options} }, name => 'TEST');
    isa_ok($parser, 'Pod::Man', 'Parser object');

    # Save stderr to a temporary file and then run the parser, storing the
    # output into a Perl variable.
    my $errors = stderr_save();
    my $got;
    $parser->output_string(\$got);
    eval { $parser->parse_string_document($data_ref->{input}) };
    my $exception = $@;
    stderr_restore();

    # Strip off everything prior to .nh from the output so that we aren't
    # testing the generated header, and then check the output.
    $got =~ s{ \A .* \n [.]nh \n }{}xms;
    is($got, $data_ref->{output}, "Output for test $n");

    # Collect the errors and add any exception, marking it with EXCEPTION.
    # Then, compare that to the expected errors.  The "1 while" construct is
    # for VMS, in case there are multiple versions of the file.
    my $got_errors = slurp($errors);
    1 while unlink($errors);
    if ($exception) {
        $exception =~ s{ [ ] at [ ] .* }{}xms;
        $got_errors .= "EXCEPTION: $exception\n";
    }
    is($got_errors, $data_ref->{errors}, "Errors for test $n");
    $n++;
}

# Below the marker are bits of POD and corresponding expected text output and
# error output.  The options, input, output, and errors are separated by lines
# containing only ###.

__DATA__

###
fixed CR
fixedbold CY
fixeditalic CW
fixedbolditalic CX
###
=head1 FIXED FONTS

C<foo B<bar I<baz>> I<bay>>
###
.SH "FIXED FONTS"
.IX Header "FIXED FONTS"
\&\f(CR\*(C`foo \f(CYbar \f(CXbaz\f(CY\f(CR \f(CWbay\f(CR\*(C'\fR
###
###

###
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
.IP "Foo" 4
.IX Item "Foo"
Bar.
.SH "NEXT"
.IX Header "NEXT"
.SH "POD ERRORS"
.IX Header "POD ERRORS"
Hey! \fBThe above document had some coding errors, which are explained below:\fR
.IP "Around line 7:" 4
.IX Item "Around line 7:"
You forgot a '=back' before '=head1'
###
###

###
stderr 1
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
.IP "Foo" 4
.IX Item "Foo"
Bar.
.SH "NEXT"
.IX Header "NEXT"
###
Pod input around line 7: You forgot a '=back' before '=head1'
###

###
nourls 1
###
=head1 URL suppression

L<anchor|http://www.example.com/>
###
.SH "URL suppression"
.IX Header "URL suppression"
anchor
###
###

###
errors stderr
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
.IP "Foo" 4
.IX Item "Foo"
Bar.
.SH "NEXT"
.IX Header "NEXT"
###
Pod input around line 7: You forgot a '=back' before '=head1'
###

###
errors die
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
.IP "Foo" 4
.IX Item "Foo"
Bar.
.SH "NEXT"
.IX Header "NEXT"
###
Pod input around line 7: You forgot a '=back' before '=head1'
EXCEPTION: POD document had syntax errors
###

###
errors pod
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
.IP "Foo" 4
.IX Item "Foo"
Bar.
.SH "NEXT"
.IX Header "NEXT"
.SH "POD ERRORS"
.IX Header "POD ERRORS"
Hey! \fBThe above document had some coding errors, which are explained below:\fR
.IP "Around line 7:" 4
.IX Item "Around line 7:"
You forgot a '=back' before '=head1'
###
###

###
errors none
###
=over 4

=item Foo

Bar.

=head1 NEXT
###
.IP "Foo" 4
.IX Item "Foo"
Bar.
.SH "NEXT"
.IX Header "NEXT"
###
###

###
errors none
###
=over 4

=item foo

Not a bullet.

=item *

Also not a bullet.

=back
###
.IP "foo" 4
.IX Item "foo"
Not a bullet.
.IP "*" 4
Also not a bullet.
###
###

###
quotes \(lq"\(rq"
###
=head1 FOO C<BAR> BAZ

Foo C<bar> baz.
###
.ie n .SH "FOO \(lq""BAR\(rq"" BAZ"
.el .SH "FOO \f(CWBAR\fP BAZ"
.IX Header "FOO BAR BAZ"
Foo \f(CW\*(C`bar\*(C'\fR baz.
###
###
