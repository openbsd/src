# just copy modules
# TODO: copy tests and try to run them...
# this file may be used as example on how to use comp.pl

my @files;

my %dirs;
sub mk {
  my $r = shift;
  return if exists $dirs{$r};
  if ($r=~/\//) {
    $r=~/^(.*)\/[^\/]*?$/;
    mk($1);
  }
  print STDERR "..\\miniperl.exe -MCross comp.pl --do cemkdir [p]\\lib\\$r\n";
  system("..\\miniperl.exe -I..\\lib -MCross comp.pl --do cemkdir [p]\\lib\\$r");
  $dirs{$r}++;
}
for (@files) {
  if (/\//) {
    /^(.*)\/[^\/]*?$/;
    mk($1);
  }
  # currently no stripping POD
  system("..\\miniperl.exe -I..\\lib -MCross comp.pl --copy pc:..\\lib\\$_ ce:[p]\\lib\\$_");
}

sub BEGIN {
 @files = qw(
    attributes.pm
    AutoLoader.pm
    AutoSplit.pm
    autouse.pm
    base.pm
    Benchmark.pm
    bigint.pm
    bignum.pm
    bigrat.pm
    blib.pm
    bytes.pm
    Carp.pm
    charnames.pm
    _charnames.pm
    Config.pm
    constant.pm
    Cwd.pm
    DB.pm
    diagnostics.pm
    Digest.pm
    DirHandle.pm
    Dumpvalue.pm
    DynaLoader.pm
    English.pm
    Env.pm
    Exporter.pm
    Fatal.pm
    fields.pm
    FileCache.pm
    FileHandle.pm
    filetest.pm
    FindBin.pm
    if.pm
    integer.pm
    less.pm
    locale.pm
    Memoize.pm
    NEXT.pm
    open.pm
    overload.pm
    PerlIO.pm
    re.pm
    SelectSaver.pm
    SelfLoader.pm
    Shell.pm
    sigtrap.pm
    sort.pm
    strict.pm
    subs.pm
    Switch.pm
    Symbol.pm
    Test.pm
    UNIVERSAL.pm
    utf8.pm
    vars.pm
    vmsish.pm
    warnings.pm
    XSLoader.pm
    warnings/register.pm
    Unicode/Collate.pm
    Unicode/UCD.pm
    Time/gmtime.pm
    Time/Local.pm
    Time/localtime.pm
    Time/tm.pm
    Tie/Array.pm
    Tie/File.pm
    Tie/Handle.pm
    Tie/Hash.pm
    Tie/Memoize.pm
    Tie/RefHash.pm
    Tie/Scalar.pm
    Tie/SubstrHash.pm
    Text/Abbrev.pm
    Text/Balanced.pm
    Text/ParseWords.pm
    Text/Soundex.pm
    Text/Tabs.pm
    Text/Wrap.pm
    Test/Builder.pm
    Test/Harness.pm
    Test/More.pm
    Test/Simple.pm
    Test/Harness/Assert.pm
    Test/Harness/Iterator.pm
    Test/Harness/Straps.pm
    Term/ANSIColor.pm
    Term/Cap.pm
    Term/Complete.pm
    Term/ReadLine.pm
    Search/Dict.pm
    Pod/Checker.pm
    Pod/Find.pm
    Pod/Functions.pm
    Pod/Html.pm
    Pod/InputObjects.pm
    Pod/LaTeX.pm
    Pod/Man.pm
    Pod/ParseLink.pm
    Pod/Parser.pm
    Pod/ParseUtils.pm
    Pod/Plainer.pm
    Pod/Select.pm
    Pod/Text.pm
    Pod/Usage.pm
    Pod/Text/Color.pm
    Pod/Text/Overstrike.pm
    Pod/Text/Termcap.pm
    Math/BigFloat.pm
    Math/BigInt.pm
    Math/BigRat.pm
    Math/Complex.pm
    Math/Trig.pm
    Math/BigInt/Calc.pm
    Math/BigInt/Trace.pm
    Math/BigFloat/Trace.pm
    Locale/Constants.pm
    Locale/Country.pm
    Locale/Currency.pm
    Locale/Language.pm
    Locale/Maketext.pm
    Locale/Script.pm
    IPC/Open2.pm
    IPC/Open3.pm
    I18N/Collate.pm
    I18N/LangTags.pm
    I18N/LangTags/List.pm
    Hash/Util.pm
    Getopt/Long.pm
    Getopt/Std.pm
    Filter/Simple.pm
    File/Basename.pm
    File/CheckTree.pm
    File/Compare.pm
    File/Copy.pm
    File/DosGlob.pm
    File/Find.pm
    File/Path.pm
    File/Spec.pm
    File/stat.pm
    File/Temp.pm
    File/Spec/Functions.pm
    File/Spec/Mac.pm
    File/Spec/Unix.pm
    File/Spec/Win32.pm
    ExtUtils/Command.pm
    ExtUtils/Constant.pm
    ExtUtils/Embed.pm
    ExtUtils/Install.pm
    ExtUtils/Installed.pm
    ExtUtils/Liblist.pm
    ExtUtils/MakeMaker.pm
    ExtUtils/Manifest.pm
    ExtUtils/Miniperl.pm
    ExtUtils/Mkbootstrap.pm
    ExtUtils/Mksymlists.pm
    ExtUtils/MM.pm
    ExtUtils/MM_Any.pm
    ExtUtils/MM_DOS.pm
    ExtUtils/MM_Unix.pm
    ExtUtils/MM_UWIN.pm
    ExtUtils/MM_Win32.pm
    ExtUtils/MM_Win95.pm
    ExtUtils/MY.pm
    ExtUtils/Packlist.pm
    ExtUtils/testlib.pm
    ExtUtils/Liblist/Kid.pm
    ExtUtils/Command/MM.pm
    Exporter/Heavy.pm
    Devel/SelfStubber.pm
    Class/ISA.pm
    Class/Struct.pm
    Carp/Heavy.pm
    Attribute/Handlers.pm
    Attribute/Handlers/demo/Demo.pm
    Attribute/Handlers/demo/Descriptions.pm
    Attribute/Handlers/demo/MyClass.pm
  );
}
