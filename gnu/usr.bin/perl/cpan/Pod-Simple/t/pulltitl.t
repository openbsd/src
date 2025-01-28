use strict;
use warnings;
use Test::More tests => 115;

#use Pod::Simple::Debug (5);

#sub Pod::Simple::MANY_LINES () {1}
#sub Pod::Simple::PullParser::DEBUG () {3}


use Pod::Simple::PullParser;

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 NAME\n\nBzorch\n\n=pod\n\nLala\n\n\=cut\n} );

ok $p->get_title(), 'Bzorch';

my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'head1' );

ok( $t = $p->get_token);
is( $t && $t->type, 'text');
is( $t && $t->type eq 'text' && $t->text, 'NAME' );

DIE: {
    # Make sure we die.
    local $@;
    eval { $p->set_source(\'=head1 foo') };
    ok $@;
    like $@, qr/\QCannot assign new source to pull parser; create a new instance, instead/;
}
}

###########################################################################

{
print "# Testing a set with nocase, at line ", __LINE__, "\n";
my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 Name\n\nShazbot\n\n=pod\n\nLala\n\n\=cut\n} );

ok $p->get_title(nocase => 1), 'Shazbot';

ok( my $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'head1' );

ok( $t = $p->get_token);
is( $t && $t->type, 'text');
is( $t && $t->type eq 'text' && $t->text, 'Name' );

}

###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 NE<65>ME\n\nBzorch\n\n=pod\n\nLala\n\n\=cut\n} );

ok $p->get_title(), 'Bzorch';
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'head1' );

ok( $t = $p->get_token);
is( $t && $t->type, 'text');

}


###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

{
my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 NAME\n\nBzorch - I<thing> lala\n\n=pod\n\nLala\n\n\=cut\n} );
is $p->get_title(), 'Bzorch - thing lala';
}


my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 NAME\n\nBzorch - I<thing> lala\n\n=pod\n\nLala\n\n\=cut\n} );
is $p->get_title(), 'Bzorch - thing lala';

my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'head1' );

ok( $t = $p->get_token);
is( $t && $t->type, 'text');
is( $t && $t->type eq 'text' && $t->text, 'NAME' );

}

###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 Bzorch lala\n\n=pod\n\nLala\n\n\=cut\n} );

is $p->get_title(), 'Bzorch lala';
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'head1' );

ok( $t = $p->get_token);
is( $t && $t->type, 'text');
is( $t && $t->type eq 'text' && $t->text, 'Bzorch lala' );

}

###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 Bzorch - I<thing> lala\n\n=pod\n\nLala\n\n\=cut\n} );

is $p->get_title(), 'Bzorch - thing lala';
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'head1' );

ok( $t = $p->get_token);
is( $t && $t->type, 'text');
is( $t && $t->type eq 'text' && $t->text, 'Bzorch - ' );

}
###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 Nombre (NAME)\n\nBzorch - I<thing> lala\n\n=pod\n\nGrunk\n\n\=cut\n} );

is $p->get_version || '', '';
is $p->get_author  || '', '';

ok $p->get_title(), 'Bzorch - thing lala';

my $t;
ok( $t = $p->get_token);
ok( $t && $t->type, 'start');
ok( $t && $t->type eq 'start' && $t->tagname, 'Document' );

}
###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 Когда читала (NAME)\n\nКогда читала ты мучительные строки -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

ok $p->get_title(), q{Когда читала ты мучительные строки -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

}

###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 (NAME) Когда читала\n\nКогда читала ты мучительные строки -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

is $p->get_title(), q{Когда читала ты мучительные строки -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

}

###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 (DESCRIPTION) Когда читала\n\nКогда читала ты мучительные строки -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

is $p->get_title() || '', '';
is $p->get_description(), q{Когда читала ты мучительные строки -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

}
###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 (DESCRIPTION) Когда читала\n\nКогда читала ты мучительные строки -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

is $p->get_description(), q{Когда читала ты мучительные строки -- Fet's "When you were reading};
is $p->get_title() || '', '';
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

}

###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 NAME\n\nThingy\n\n=head1 (DESCRIPTION) Когда читала\n\nКогда читала ты мучительные строки -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

is $p->get_description(), q{Когда читала ты мучительные строки -- Fet's "When you were reading};
is $p->get_title(), "Thingy";
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

}

###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 NAME\n\nThingy\n\n=head1 (DESCRIPTION) Когда читала\n\nКогда читала ты мучительные строки -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

is $p->get_title(), "Thingy";
is $p->get_description(), q{Когда читала ты мучительные строки -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

}

###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 (NAME) Вдали перед\n\nThingy\n\n=head1 (DESCRIPTION) Когда читала\n\nКогда читала ты мучительные строки -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

is $p->get_title(), "Thingy";
is $p->get_description(), q{Когда читала ты мучительные строки -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

}

###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \q{

=head1 (NAME) Вдали перед

Thingy

=head1 (DESCRIPTION) Когда читала

Когда читала ты мучительные строки -- Fet's I<"When you were> reading

=pod

Grunk

=cut
} );

is $p->get_title(), "Thingy";
is $p->get_version() || '', '';
is $p->get_description(), q{Когда читала ты мучительные строки -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

}

###########################################################################

{
print "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \q{

=head1 (NAME) Вдали перед

Thingy

=head1 (DESCRIPTION) Когда читала

Когда читала ты мучительные строки -- Fet's I<"When you were> reading

=head1 VERSION

  Stuff: Thing
  Whatever: Um.

=head1 AUTHOR

Jojoj E<65>arzarz

=pod

Grunk

=cut
} );

is $p->get_title(), "Thingy";
my $v = $p->get_version || '';
$v =~ s/^ +//m;
$v =~ s/^\s+//s;
$v =~ s/\s+$//s;
is $v, "Stuff: Thing\nWhatever: Um.";
is $p->get_description(), q{Когда читала ты мучительные строки -- Fet's "When you were reading};
is $p->get_author() || '', 'Jojoj Aarzarz';


my $t;
ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

}

###########################################################################
{
print "# Testing a title with an X<>, at line ", __LINE__, "\n";
my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 NAME Foo Bar\nX<Some entry>\n} );

is $p->get_title(), 'NAME Foo Bar';
}
