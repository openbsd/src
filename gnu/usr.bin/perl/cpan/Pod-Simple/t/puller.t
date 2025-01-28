use strict;
use warnings;
use Test::More tests => 135;

#use Pod::Simple::Debug (5);

#sub Pod::Simple::MANY_LINES () {1}
#sub Pod::Simple::PullParser::DEBUG () {1}


use Pod::Simple::PullParser;

sub pump_it_up {
  my $p = Pod::Simple::PullParser->new;
  $p->set_source( \( $_[0] ) );
  my(@t, $t);
  while($t = $p->get_token) { push @t, $t }
  print "# Count of tokens: ", scalar(@t), "\n";
  print "#  I.e., {", join("\n#       + ",
    map ref($_) . ": " . $_->dump, @t), "} \n";
  return @t;
}

my @t;

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

@t = pump_it_up(qq{\n\nProk\n\n=head1 Things\n\n=cut\n\nBzorch\n\n});

if(not(
  is scalar( grep { ref $_ and $_->can('type') } @t), 5
)) {
  fail "Wrong token count. Failing subsequent tests.\n";
  for ( 2 .. 12 ) {fail}
} else {
  is $t[0]->type, 'start';
  is $t[1]->type, 'start';
  is $t[2]->type, 'text';
  is $t[3]->type, 'end';
  is $t[4]->type, 'end';

  is $t[0]->tagname, 'Document';
  is $t[1]->tagname, 'head1';
  is $t[2]->text,    'Things';
  is $t[3]->tagname, 'head1';
  is $t[4]->tagname, 'Document';

  is $t[0]->attr('start_line'), '5';
  is $t[1]->attr('start_line'), '5';
}



#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
@t = pump_it_up(
    qq{Woowoo\n\n=over\n\n=item *\n\nStuff L<HTML::TokeParser>\n\n}
  . qq{=item *\n\nThings I<like that>\n\n=back\n\n=cut\n\n}
);

if(
  not( is scalar( grep { ref $_ and $_->can('type') } @t) => 16 )
) {
  fail "Wrong token count. Failing subsequent tests.\n";
  for ( 1 .. 32 ) {ok 0}
} else {
  is $t[ 0]->type, 'start';
  is $t[ 1]->type, 'start';
  is $t[ 2]->type, 'start';
  is $t[ 3]->type, 'text';
  is $t[ 4]->type, 'start';
  is $t[ 5]->type, 'text';
  is $t[ 6]->type, 'end';
  is $t[ 7]->type, 'end';

  is $t[ 8]->type, 'start';
  is $t[ 9]->type, 'text';
  is $t[10]->type, 'start';
  is $t[11]->type, 'text';
  is $t[12]->type, 'end';
  is $t[13]->type, 'end';
  is $t[14]->type, 'end';
  is $t[15]->type, 'end';



  is $t[ 0]->tagname, 'Document';
  is $t[ 1]->tagname, 'over-bullet';
  is $t[ 2]->tagname, 'item-bullet';
  is $t[ 3]->text, 'Stuff ';
  is $t[ 4]->tagname, 'L';
  is $t[ 5]->text, 'HTML::TokeParser';
  is $t[ 6]->tagname, 'L';
  is $t[ 7]->tagname, 'item-bullet';

  is $t[ 8]->tagname, 'item-bullet';
  is $t[ 9]->text, 'Things ';
  is $t[10]->tagname, 'I';
  is $t[11]->text, 'like that';
  is $t[12]->tagname, 'I';
  is $t[13]->tagname, 'item-bullet';
  is $t[14]->tagname, 'over-bullet';
  is $t[15]->tagname, 'Document';

  is $t[4]->attr("type"), "pod";
}


#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
{
print "# Testing unget_token\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\nBzorch\n\n=pod\n\nLala\n\n\=cut\n} );

ok 1;
my $t;
$t = $p->get_token;
is $t && $t->type, 'start';
is $t && $t->tagname, 'Document';
print "# ungetting ($t).\n";
$p->unget_token($t);
ok 1;

$t = $p->get_token;
is $t && $t->type, 'start';
is $t && $t->tagname, 'Document';
my @to_save = ($t);

$t = $p->get_token;
is $t && $t->type, 'start';
is $t && $t->tagname, 'Para';
push @to_save, $t;

print "# ungetting (@to_save).\n";
$p->unget_token(@to_save);
splice @to_save;


$t = $p->get_token;
is $t && $t->type, 'start';
is $t && $t->tagname, 'Document';

$t = $p->get_token;
is $t && $t->type, 'start';
is $t && $t->tagname, 'Para';

ok 1;

}


#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

{
print "# Testing pullparsing from an arrayref\n";
my $p = Pod::Simple::PullParser->new;
ok 1;
$p->set_source( ['','Bzorch', '','=pod', '', 'Lala', 'zaza', '', '=cut'] );
ok 1;
my( @t, $t );
while($t = $p->get_token) {
  print "# Got a token: ", $t->dump, "\n#\n";
  push @t, $t;
}
is scalar(@t), 5; # count of tokens
is $t[0]->type, 'start';
is $t[1]->type, 'start';
is $t[2]->type, 'text';
is $t[3]->type, 'end';
is $t[4]->type, 'end';

is $t[0]->tagname, 'Document';
is $t[1]->tagname, 'Para';
is $t[2]->text,    'Lala zaza';
is $t[3]->tagname, 'Para';
is $t[4]->tagname, 'Document';

}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

{
print "# Testing pullparsing from an arrayref with terminal newlines\n";
my $p = Pod::Simple::PullParser->new;
ok 1;
$p->set_source( [ map "$_\n",
  '','Bzorch', '','=pod', '', 'Lala', 'zaza', '', '=cut'] );
ok 1;
my( @t, $t );
while($t = $p->get_token) {
  print "# Got a token: ", $t->dump, "\n#\n";
  push @t, $t;
}
is scalar(@t), 5; # count of tokens
is $t[0]->type, 'start';
is $t[1]->type, 'start';
is $t[2]->type, 'text';
is $t[3]->type, 'end';
is $t[4]->type, 'end';

is $t[0]->tagname, 'Document';
is $t[1]->tagname, 'Para';
is $t[2]->text,    'Lala zaza';
is $t[3]->tagname, 'Para';
is $t[4]->tagname, 'Document';

}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
our $temp_pod = "temp_$$.pod";
END { unlink "$temp_pod" }
{
print "# Testing pullparsing from a file\n";
my $p = Pod::Simple::PullParser->new;
ok 1;
open(OUT, ">$temp_pod") || die "Can't write-open $temp_pod: $!";
print OUT
 map "$_\n",
  '','Bzorch', '','=pod', '', 'Lala', 'zaza', '', '=cut'
;
close(OUT);
ok 1;
sleep 1;

$p->set_source("$temp_pod");

my( @t, $t );
while($t = $p->get_token) {
  print "# Got a token: ", $t->dump, "\n#\n";
  push @t, $t;
  print "#  That's token number ", scalar(@t), "\n";
}
is scalar(@t), 5; # count of tokens
is $t[0]->type, 'start';
is $t[1]->type, 'start';
is $t[2]->type, 'text';
is $t[3]->type, 'end';
is $t[4]->type, 'end';

is $t[0]->tagname, 'Document';
is $t[1]->tagname, 'Para';
is $t[2]->text,    'Lala zaza';
is $t[3]->tagname, 'Para';
is $t[4]->tagname, 'Document';

}

# ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~

{
print "# Testing pullparsing from a glob\n";
my $p = Pod::Simple::PullParser->new;
ok 1;
open(IN, "<$temp_pod") || die "Can't read-open $temp_pod: $!";
$p->set_source(*IN);

my( @t, $t );
while($t = $p->get_token) {
  print "# Got a token: ", $t->dump, "\n#\n";
  push @t, $t;
  print "#  That's token number ", scalar(@t), "\n";
}
is scalar(@t), 5; # count of tokens
is $t[0]->type, 'start';
is $t[1]->type, 'start';
is $t[2]->type, 'text';
is $t[3]->type, 'end';
is $t[4]->type, 'end';

is $t[0]->tagname, 'Document';
is $t[1]->tagname, 'Para';
is $t[2]->text,    'Lala zaza';
is $t[3]->tagname, 'Para';
is $t[4]->tagname, 'Document';
close(IN);

}

# ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~

{
print "# Testing pullparsing from a globref\n";
my $p = Pod::Simple::PullParser->new;
ok 1;
open(IN, "<$temp_pod") || die "Can't read-open $temp_pod: $!";
$p->set_source(\*IN);

my( @t, $t );
while($t = $p->get_token) {
  print "# Got a token: ", $t->dump, "\n#\n";
  push @t, $t;
  print "#  That's token number ", scalar(@t), "\n";
}
is scalar(@t), 5; # count of tokens
is $t[0]->type, 'start';
is $t[1]->type, 'start';
is $t[2]->type, 'text';
is $t[3]->type, 'end';
is $t[4]->type, 'end';

is $t[0]->tagname, 'Document';
is $t[1]->tagname, 'Para';
is $t[2]->text,    'Lala zaza';
is $t[3]->tagname, 'Para';
is $t[4]->tagname, 'Document';
close(IN);

}

# ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~

{
print "# Testing pullparsing from a filehandle\n";
my $p = Pod::Simple::PullParser->new;
ok 1;
open(IN, "<$temp_pod") || die "Can't read-open $temp_pod: $!";
$p->set_source(*IN{IO});

my( @t, $t );
while($t = $p->get_token) {
  print "# Got a token: ", $t->dump, "\n#\n";
  push @t, $t;
  print "#  That's token number ", scalar(@t), "\n";
}
is scalar(@t), 5; # count of tokens
is $t[0]->type, 'start';
is $t[1]->type, 'start';
is $t[2]->type, 'text';
is $t[3]->type, 'end';
is $t[4]->type, 'end';

is $t[0]->tagname, 'Document';
is $t[1]->tagname, 'Para';
is $t[2]->text,    'Lala zaza';
is $t[3]->tagname, 'Para';
is $t[4]->tagname, 'Document';
close(IN);

}
