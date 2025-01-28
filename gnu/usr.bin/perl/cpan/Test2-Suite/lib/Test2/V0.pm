package Test2::V0;
use strict;
use warnings;

use Test2::Util::Importer;

our $VERSION = '0.000162';

use Carp qw/croak/;

use Test2::Plugin::SRand();
use Test2::Plugin::UTF8();
use Test2::Tools::Target();

use Test2::Plugin::ExitSummary;

use Test2::API qw/intercept context/;

use Test2::Tools::Event qw/gen_event/;

use Test2::Tools::Defer qw/def do_def/;

use Test2::Tools::Basic qw{
    ok pass fail diag note todo skip
    plan skip_all done_testing bail_out
};

use Test2::Tools::Compare qw{
    is like isnt unlike
    match mismatch validator
    hash array bag object meta meta_check number float rounded within string subset bool check_isa
    number_lt number_le number_ge number_gt
    in_set not_in_set check_set
    item field call call_list call_hash prop check all_items all_keys all_vals all_values
    etc end filter_items
    T F D DF E DNE FDNE U L
    event fail_events
    exact_ref
};

use Test2::Tools::Warnings qw{
    warns warning warnings no_warnings
};

use Test2::Tools::ClassicCompare qw/cmp_ok/;

use Test2::Util::Importer 'Test2::Tools::Subtest' => (
    subtest_buffered => { -as => 'subtest' },
);

use Test2::Tools::Class     qw/can_ok isa_ok DOES_ok/;
use Test2::Tools::Encoding  qw/set_encoding/;
use Test2::Tools::Exports   qw/imported_ok not_imported_ok/;
use Test2::Tools::Ref       qw/ref_ok ref_is ref_is_not/;
use Test2::Tools::Mock      qw/mock mocked/;
use Test2::Tools::Exception qw/try_ok dies lives/;
use Test2::Tools::Refcount  qw/is_refcount is_oneref refcount/;

our @EXPORT = qw{
    ok pass fail diag note todo skip
    plan skip_all done_testing bail_out

    intercept context

    gen_event

    def do_def

    cmp_ok

    warns warning warnings no_warnings

    subtest
    can_ok isa_ok DOES_ok
    set_encoding
    imported_ok not_imported_ok
    ref_ok ref_is ref_is_not
    mock mocked
    dies lives try_ok

    is like isnt unlike
    match mismatch validator
    hash array bag object meta meta_check number float rounded within string subset bool check_isa
    number_lt number_le number_ge number_gt
    in_set not_in_set check_set
    item field call call_list call_hash prop check all_items all_keys all_vals all_values
    etc end filter_items
    T F D DF E DNE FDNE U L
    event fail_events
    exact_ref

    is_refcount is_oneref refcount
};

my $SRAND;
sub import {
    my $class = shift;

    my $caller = caller;
    my (@exports, %options);
    while (my $arg = shift @_) {
        push @exports => $arg and next unless substr($arg, 0, 1) eq '-';
        $options{$arg} = shift @_;
    }

    # SRand handling
    my $srand    = delete $options{'-srand'};

    my $no_srand = exists $options{'-no_srand'};
    delete $options{'-no_srand'} if $no_srand;

    croak "Cannot combine '-srand' and '-no_srand' options"
        if $no_srand && defined($srand);

    if ( !$no_srand ) {
        Test2::Plugin::SRand->import($srand ? $srand : ()) if defined($srand) || !$SRAND++;
    }

    # Pragmas
    my $no_pragmas  = delete $options{'-no_pragmas'};
    my $no_strict   = delete $options{'-no_strict'} || $no_pragmas;
    my $no_warnings = delete $options{'-no_warnings'} || $no_pragmas;
    my $no_utf8     = delete $options{'-no_utf8'} || $no_pragmas;

    strict->import()              unless $no_strict;
    'warnings'->import()          unless $no_warnings;
    Test2::Plugin::UTF8->import() unless $no_utf8;

    my $target = delete $options{'-target'};
    Test2::Tools::Target->import_into($caller, $target)
        if $target;

    croak "Unknown option(s): " . join(', ', sort keys %options) if keys %options;

    Test2::Util::Importer->import_into($class, $caller, @exports);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::V0 - 0Th edition of the Test2 recommended bundle.

=head1 DESCRIPTION

This is the big-daddy bundle. This bundle includes nearly every tool, and
several plugins, that the Test2 author uses. This bundle is used
extensively to test L<Test2::Suite> itself.

=head1 NAMING, USING, DEPENDING

This bundle should not change in a I<severely> incompatible way. Some minor
breaking changes, specially bugfixes, may be allowed. If breaking changes are
needed then a new C<Test2::V#> module should be released instead.

As new C<V#> modules are released old ones I<may> be moved to different cpan
distributions. You should always use a specific bundle version and list that
version in your distributions testing requirements. You should never simply
list L<Test2::Suite> as your modules dep, instead list the specific bundle, or
tools and plugins you use directly in your metadata.

=head1 SYNOPSIS

    use Test2::V0;

    ok(1, "pass");

    ...

    done_testing;

=head1 RESOLVING CONFLICTS WITH MOOSE

    use Test2::V0 '!meta';

L<Moose> and L<Test2::V0> both export very different C<meta()>
subs. Adding C<'!meta'> to the import args will prevent the sub from being
imported. This bundle also exports the sub under the name C<meta_check()> so
you can use that spelling as an alternative.

=head2 TAGS

=over 4

=item :DEFAULT

The following are both identical:

    use Test2::V0;

    use Test2::V0 ':DEFAULT';

=back

=head2 RENAMING ON IMPORT

    use Test2::V0 ':DEFAULT', '!ok', ok => {-as => 'my_ok'};

This bundle uses L<Test2::Util::Importer> for exporting, as such you can use any arguments
it accepts.

Explanation:

=over 4

=item '!ok'

Do not export C<ok()>

=item ok => {-as => 'my_ok'}

Actually, go ahead and import C<ok()> but under the name C<my_ok()>.

=back

If you did not add the C<'!ok'> argument then you would have both C<ok()> and
C<my_ok()>

=head1 PRAGMAS

All of these can be disabled via individual import arguments, or by the
C<-no_pragmas> argument.

    use Test2::V0 -no_pragmas => 1;

=head2 STRICT

L<strict> is turned on for you. You can disable this with the C<-no_strict> or
C<-no_pragmas> import arguments:

    use Test2::V0 -no_strict => 1;

=head2 WARNINGS

L<warnings> are turned on for you. You can disable this with the
C<-no_warnings> or C<-no_pragmas> import arguments:

    use Test2::V0 -no_warnings => 1;

=head2 UTF8

This is actually done via the L<Test2::Plugin::UTF8> plugin, see the
L</PLUGINS> section for details.

B<Note:> C<< -no_pragmas => 1 >> will turn off the entire plugin.

=head1 PLUGINS

=head2 SRAND

See L<Test2::Plugin::SRand>.

This will set the random seed to today's date. You can provide an alternate seed
with the C<-srand> import option:

    use Test2::V0 -srand => 1234;

You can also disable this behavior:

    use Test2::V0 -no_srand => 1;

B<Note> When srand is on (default) it can cause problems with things like
L<File::Temp> which will end up attempting the same "random" filenames for
every test process started on a given day (or sharing the same seed).

=head2 UTF8

See L<Test2::Plugin::UTF8>.

This will set the file, and all output handles (including formatter handles), to
utf8. This will turn on the utf8 pragma for the current scope.

This can be disabled using the C<< -no_utf8 => 1 >> or C<< -no_pragmas => 1 >>
import arguments.

    use Test2::V0 -no_utf8 => 1;

=head2 EXIT SUMMARY

See L<Test2::Plugin::ExitSummary>.

This plugin has no configuration.

=head1 API FUNCTIONS

See L<Test2::API> for these

=over 4

=item $ctx = context()

=item $events = intercept { ... }

=back

=head1 TOOLS

=head2 TARGET

See L<Test2::Tools::Target>.

You can specify a target class with the C<-target> import argument. If you do
not provide a target then C<$CLASS> and C<CLASS()> will not be imported.

    use Test2::V0 -target => 'My::Class';

    print $CLASS;  # My::Class
    print CLASS(); # My::Class

Or you can specify names:

    use Test2::V0 -target => { pkg => 'Some::Package' };

    pkg()->xxx; # Call 'xxx' on Some::Package
    $pkg->xxx;  # Same

=over 4

=item $CLASS

Package variable that contains the target class name.

=item $class = CLASS()

Constant function that returns the target class name.

=back

=head2 DEFER

See L<Test2::Tools::Defer>.

=over 4

=item def $func => @args;

=item do_def()

=back

=head2 BASIC

See L<Test2::Tools::Basic>.

=over 4

=item ok($bool, $name)

=item ok($bool, $name, @diag)

=item pass($name)

=item pass($name, @diag)

=item fail($name)

=item fail($name, @diag)

=item diag($message)

=item note($message)

=item $todo = todo($reason)

=item todo $reason => sub { ... }

=item skip($reason, $count)

=item plan($count)

=item skip_all($reason)

=item done_testing()

=item bail_out($reason)

=back

=head2 COMPARE

See L<Test2::Tools::Compare>.

=over 4

=item is($got, $want, $name)

=item isnt($got, $do_not_want, $name)

=item like($got, qr/match/, $name)

=item unlike($got, qr/mismatch/, $name)

=item $check = match(qr/pattern/)

=item $check = mismatch(qr/pattern/)

=item $check = validator(sub { return $bool })

=item $check = hash { ... }

=item $check = array { ... }

=item $check = bag { ... }

=item $check = object { ... }

=item $check = meta { ... }

=item $check = number($num)

=item $check = string($str)

=item $check = bool($bool)

=item $check = check_isa($class_name)

=item $check = in_set(@things)

=item $check = not_in_set(@things)

=item $check = check_set(@things)

=item $check = item($thing)

=item $check = item($idx => $thing)

=item $check = field($name => $val)

=item $check = call($method => $expect)

=item $check = call_list($method => $expect)

=item $check = call_hash($method => $expect)

=item $check = prop($name => $expect)

=item $check = check($thing)

=item $check = T()

=item $check = F()

=item $check = D()

=item $check = DF()

=item $check = E()

=item $check = DNE()

=item $check = FDNE()

=item $check = U()

=item $check = L()

=item $check = exact_ref($ref)

=item end()

=item etc()

=item filter_items { grep { ... } @_ }

=item $check = event $type => ...

=item @checks = fail_events $type => ...

=back

=head2 CLASSIC COMPARE

See L<Test2::Tools::ClassicCompare>.

=over 4

=item cmp_ok($got, $op, $want, $name)

=back

=head2 SUBTEST

See L<Test2::Tools::Subtest>.

=over 4

=item subtest $name => sub { ... };

(Note: This is called C<subtest_buffered()> in the Tools module.)

=back

=head2 CLASS

See L<Test2::Tools::Class>.

=over 4

=item can_ok($thing, @methods)

=item isa_ok($thing, @classes)

=item DOES_ok($thing, @roles)

=back

=head2 ENCODING

See L<Test2::Tools::Encoding>.

=over 4

=item set_encoding($encoding)

=back

=head2 EXPORTS

See L<Test2::Tools::Exports>.

=over 4

=item imported_ok('function', '$scalar', ...)

=item not_imported_ok('function', '$scalar', ...)

=back

=head2 REF

See L<Test2::Tools::Ref>.

=over 4

=item ref_ok($ref, $type)

=item ref_is($got, $want)

=item ref_is_not($got, $do_not_want)

=back

See L<Test2::Tools::Refcount>.

=over 4

=item is_refcount($ref, $count, $description)

=item is_oneref($ref, $description)

=item $count = refcount($ref)

=back

=head2 MOCK

See L<Test2::Tools::Mock>.

=over 4

=item $control = mock ...

=item $bool = mocked($thing)

=back

=head2 EXCEPTION

See L<Test2::Tools::Exception>.

=over 4

=item $exception = dies { ... }

=item $bool = lives { ... }

=item $bool = try_ok { ... }

=back

=head2 WARNINGS

See L<Test2::Tools::Warnings>.

=over 4

=item $count = warns { ... }

=item $warning = warning { ... }

=item $warnings_ref = warnings { ... }

=item $bool = no_warnings { ... }

=back

=head1 SOURCE

The source code repository for Test2-Suite can be found at
F<https://github.com/Test-More/Test2-Suite/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2018 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
