package Test2::Manual::Tooling;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Tooling - Manual page for tool authors.

=head1 DESCRIPTION

This section covers writing new tools, plugins, and other Test2 components.

=head1 TOOL TUTORIALS

=head2 FIRST TOOL

L<Test2::Manual::Tooling::FirstTool> - Introduction to writing tools by cloning
L<ok()>.

=head2 MOVING FROM Test::Builder

L<Test2::Manual::Tooling::TestBuilder> - This section maps Test::Builder
methods to Test2 concepts.

=head2 NESTING TOOLS

L<Test2::Manual::Tooling::Nesting> - How to call other tools from your tool.

=head2 TOOLS WITH SUBTESTS

L<Test2::Manual::Tooling::Subtest> - How write tools that make use of subtests.

=head2 TESTING YOUR TEST TOOLS

L<Test2::Manual::Tooling::Testing> - How to write tests for your test tools.

=head1 PLUGIN TUTORIALS

=head2 TAKING ACTION WHEN A NEW TOOL STARTS

L<Test2::Manual::Tooling::Plugin::ToolStarts> - How to add behaviors that occur
when a tool starts work.

=head2 TAKING ACTION AFTER A TOOL IS DONE

L<Test2::Manual::Tooling::Plugin::ToolCompletes> - How to add behaviors that
occur when a tool completes work.

=head2 TAKING ACTION AT THE END OF TESTING

L<Test2::Manual::Tooling::Plugin::TestingDone> - How to add behaviors that
occur when testing is complete (IE done_testing, or end of test).

=head2 TAKING ACTION JUST BEFORE EXIT

L<Test2::Manual::Tooling::Plugin::TestExit> - How to safely add pre-exit
behaviors.

=head1 WRITING A SIMPLE JSONL FORMATTER

L<Test2::Manual::Tooling::Formatter> - How to write a custom formatter, in our
case a JSONL formatter.

=head1 WHERE TO FIND HOOKS AND APIS

=over 4

=item global API

L<Test2::API> is the global API. This is primarily used by plugins that provide
global behavior.

=item In hubs

L<Test2::Hub> is the base class for all hubs. This is where hooks for
manipulating events, or running things at the end of testing live.

=back

=head1 SEE ALSO

L<Test2::Manual> - Primary index of the manual.

=head1 SOURCE

The source code repository for Test2-Manual can be found at
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
