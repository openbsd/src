
# Time-stamp: "2004-03-30 17:46:17 AST"

use Test;
BEGIN { plan tests => 26 };
print "#\n# Testing normal (tight) insertion of super-ordinate language tags...\n#\n";

use I18N::LangTags qw(implicate_supers);

my @in = grep m/\S/, split /[\n\r]/, q{
 NIX => NIX
  sv => sv
  en => en
 hai => hai

          pt-br => pt-br pt
       pt-br fr => pt-br pt fr
    pt-br fr pt => pt-br fr pt

 pt-br fr pt de => pt-br fr pt de
 de pt-br fr pt => de pt-br fr pt
    de pt-br fr => de pt-br pt fr
   hai pt-br fr => hai pt-br pt fr

 # Now test multi-part complicateds:
          pt-br-janeiro => pt-br-janeiro pt-br pt
       pt-br-janeiro fr => pt-br-janeiro pt-br pt fr
    pt-br-janeiro de fr => pt-br-janeiro pt-br pt de fr
 pt-br-janeiro de pt fr => pt-br-janeiro pt-br de pt fr

          pt-br-janeiro pt-br-saopaolo => pt-br-janeiro pt-br pt pt-br-saopaolo
       pt-br-janeiro fr pt-br-saopaolo => pt-br-janeiro pt-br pt fr pt-br-saopaolo
    pt-br-janeiro de pt-br-saopaolo fr => pt-br-janeiro pt-br pt de pt-br-saopaolo fr
    pt-br-janeiro de pt-br fr pt-br-saopaolo => pt-br-janeiro de pt-br pt fr pt-br-saopaolo

 pt-br de en fr pt-br-janeiro => pt-br pt de en fr pt-br-janeiro
 pt-br de en fr               => pt-br pt de en fr

    ja    pt-br-janeiro fr => ja pt-br-janeiro pt-br pt fr
    ja pt-br-janeiro de fr => ja pt-br-janeiro pt-br pt de fr
 ja pt-br-janeiro de pt fr => ja pt-br-janeiro pt-br de pt fr

 pt-br-janeiro de pt-br fr => pt-br-janeiro de pt-br pt fr
# an odd case, since we don't filter for uniqueness in this sub
 
};

sub uniq { my %seen; return grep(!($seen{$_}++), @_); }

foreach my $in (@in) {
  $in =~ s/^\s+//s;
  $in =~ s/\s+$//s;
  $in =~ s/#.+//s;
  next unless $in =~ m/\S/;
  
  my(@in, @should);
  {
    die "What kind of line is <$in>?!"
     unless $in =~ m/^(.+)=>(.+)$/s;
  
    my($i,$s) = ($1, $2);
    @in     = ($i =~ m/(\S+)/g);
    @should = ($s =~ m/(\S+)/g);
    #print "{@in}{@should}\n";
  }
  my @out = implicate_supers(
    ("@in" eq 'NIX') ? () : @in
  );
  #print "O: ", join(' ', map "<$_>", @out), "\n";
  @out = 'NIX' unless @out;

  
  if( @out == @should
      and lc( join "\e", @out ) eq lc( join "\e", @should )
  ) {
    print "#     Happily got [@out] from [$in]\n";
    ok 1;
  } else {
    ok 0;
    print "#!!Got:         [@out]\n",
          "#!! but wanted: [@should]\n",
          "#!! from \"$in\"\n#\n";
  }
}

print "#\n#\n# Bye-bye!\n";
ok 1;

