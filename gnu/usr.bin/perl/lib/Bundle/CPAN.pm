package Bundle::CPAN;

$VERSION = '0.03';

1;

__END__

=head1 NAME

Bundle::CPAN - A bundle to play with all the other modules on CPAN

=head1 SYNOPSIS

C<perl -MCPAN -e 'install Bundle::CPAN'>

=head1 CONTENTS

MD5

Data::Dumper  # Bundle::libnet may have problems to work without it

Bundle::libnet

Term::ReadKey

Term::ReadLine::Perl # sorry, I'm discriminating the ::Gnu module

CPAN::WAIT

CPAN

=head1 DESCRIPTION

This bundle includes CPAN.pm as the base module and CPAN::WAIT, the
first plugin for CPAN that was developed even before there was an API.

After installing this bundle, it is recommended to quit the current
session and start again in a new process to enable Term::ReadLine.

=head1 AUTHOR

Andreas König
