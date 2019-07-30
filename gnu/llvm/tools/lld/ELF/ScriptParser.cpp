//===- ScriptParser.cpp ---------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains a recursive-descendent parser for linker scripts.
// Parsed results are stored to Config and Script global objects.
//
//===----------------------------------------------------------------------===//

#include "ScriptParser.h"
#include "Config.h"
#include "Driver.h"
#include "InputSection.h"
#include "LinkerScript.h"
#include "Memory.h"
#include "OutputSections.h"
#include "ScriptLexer.h"
#include "Symbols.h"
#include "Target.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include <cassert>
#include <limits>
#include <vector>

using namespace llvm;
using namespace llvm::ELF;
using namespace llvm::support::endian;
using namespace lld;
using namespace lld::elf;

static bool isUnderSysroot(StringRef Path);

namespace {
class ScriptParser final : ScriptLexer {
public:
  ScriptParser(MemoryBufferRef MB)
      : ScriptLexer(MB),
        IsUnderSysroot(isUnderSysroot(MB.getBufferIdentifier())) {}

  void readLinkerScript();
  void readVersionScript();
  void readDynamicList();

private:
  void addFile(StringRef Path);
  OutputSection *checkSection(OutputSectionCommand *Cmd, StringRef Loccation);

  void readAsNeeded();
  void readEntry();
  void readExtern();
  void readGroup();
  void readInclude();
  void readMemory();
  void readOutput();
  void readOutputArch();
  void readOutputFormat();
  void readPhdrs();
  void readSearchDir();
  void readSections();
  void readVersion();
  void readVersionScriptCommand();

  SymbolAssignment *readAssignment(StringRef Name);
  BytesDataCommand *readBytesDataCommand(StringRef Tok);
  uint32_t readFill();
  uint32_t parseFill(StringRef Tok);
  void readSectionAddressType(OutputSectionCommand *Cmd);
  OutputSectionCommand *readOutputSectionDescription(StringRef OutSec);
  std::vector<StringRef> readOutputSectionPhdrs();
  InputSectionDescription *readInputSectionDescription(StringRef Tok);
  StringMatcher readFilePatterns();
  std::vector<SectionPattern> readInputSectionsList();
  InputSectionDescription *readInputSectionRules(StringRef FilePattern);
  unsigned readPhdrType();
  SortSectionPolicy readSortKind();
  SymbolAssignment *readProvideHidden(bool Provide, bool Hidden);
  SymbolAssignment *readProvideOrAssignment(StringRef Tok);
  void readSort();
  AssertCommand *readAssert();
  Expr readAssertExpr();

  uint64_t readMemoryAssignment(StringRef, StringRef, StringRef);
  std::pair<uint32_t, uint32_t> readMemoryAttributes();

  Expr readExpr();
  Expr readExpr1(Expr Lhs, int MinPrec);
  StringRef readParenLiteral();
  Expr readPrimary();
  Expr readTernary(Expr Cond);
  Expr readParenExpr();

  // For parsing version script.
  std::vector<SymbolVersion> readVersionExtern();
  void readAnonymousDeclaration();
  void readVersionDeclaration(StringRef VerStr);

  std::pair<std::vector<SymbolVersion>, std::vector<SymbolVersion>>
  readSymbols();

  bool IsUnderSysroot;
};
} // namespace

static StringRef unquote(StringRef S) {
  if (S.startswith("\""))
    return S.substr(1, S.size() - 2);
  return S;
}

static bool isUnderSysroot(StringRef Path) {
  if (Config->Sysroot == "")
    return false;
  for (; !Path.empty(); Path = sys::path::parent_path(Path))
    if (sys::fs::equivalent(Config->Sysroot, Path))
      return true;
  return false;
}

// Some operations only support one non absolute value. Move the
// absolute one to the right hand side for convenience.
static void moveAbsRight(ExprValue &A, ExprValue &B) {
  if (A.isAbsolute())
    std::swap(A, B);
  if (!B.isAbsolute())
    error(A.Loc + ": at least one side of the expression must be absolute");
}

static ExprValue add(ExprValue A, ExprValue B) {
  moveAbsRight(A, B);
  return {A.Sec, A.ForceAbsolute, A.Val + B.getValue(), A.Loc};
}

static ExprValue sub(ExprValue A, ExprValue B) {
  return {A.Sec, A.Val - B.getValue(), A.Loc};
}

static ExprValue mul(ExprValue A, ExprValue B) {
  return A.getValue() * B.getValue();
}

static ExprValue div(ExprValue A, ExprValue B) {
  if (uint64_t BV = B.getValue())
    return A.getValue() / BV;
  error("division by zero");
  return 0;
}

static ExprValue bitAnd(ExprValue A, ExprValue B) {
  moveAbsRight(A, B);
  return {A.Sec, A.ForceAbsolute,
          (A.getValue() & B.getValue()) - A.getSecAddr(), A.Loc};
}

static ExprValue bitOr(ExprValue A, ExprValue B) {
  moveAbsRight(A, B);
  return {A.Sec, A.ForceAbsolute,
          (A.getValue() | B.getValue()) - A.getSecAddr(), A.Loc};
}

void ScriptParser::readDynamicList() {
  expect("{");
  readAnonymousDeclaration();
  if (!atEOF())
    setError("EOF expected, but got " + next());
}

void ScriptParser::readVersionScript() {
  readVersionScriptCommand();
  if (!atEOF())
    setError("EOF expected, but got " + next());
}

void ScriptParser::readVersionScriptCommand() {
  if (consume("{")) {
    readAnonymousDeclaration();
    return;
  }

  while (!atEOF() && !Error && peek() != "}") {
    StringRef VerStr = next();
    if (VerStr == "{") {
      setError("anonymous version definition is used in "
               "combination with other version definitions");
      return;
    }
    expect("{");
    readVersionDeclaration(VerStr);
  }
}

void ScriptParser::readVersion() {
  expect("{");
  readVersionScriptCommand();
  expect("}");
}

void ScriptParser::readLinkerScript() {
  while (!atEOF()) {
    StringRef Tok = next();
    if (Tok == ";")
      continue;

    if (Tok == "ASSERT") {
      Script->Opt.Commands.push_back(readAssert());
    } else if (Tok == "ENTRY") {
      readEntry();
    } else if (Tok == "EXTERN") {
      readExtern();
    } else if (Tok == "GROUP" || Tok == "INPUT") {
      readGroup();
    } else if (Tok == "INCLUDE") {
      readInclude();
    } else if (Tok == "MEMORY") {
      readMemory();
    } else if (Tok == "OUTPUT") {
      readOutput();
    } else if (Tok == "OUTPUT_ARCH") {
      readOutputArch();
    } else if (Tok == "OUTPUT_FORMAT") {
      readOutputFormat();
    } else if (Tok == "PHDRS") {
      readPhdrs();
    } else if (Tok == "SEARCH_DIR") {
      readSearchDir();
    } else if (Tok == "SECTIONS") {
      readSections();
    } else if (Tok == "VERSION") {
      readVersion();
    } else if (SymbolAssignment *Cmd = readProvideOrAssignment(Tok)) {
      Script->Opt.Commands.push_back(Cmd);
    } else {
      setError("unknown directive: " + Tok);
    }
  }
}

void ScriptParser::addFile(StringRef S) {
  if (IsUnderSysroot && S.startswith("/")) {
    SmallString<128> PathData;
    StringRef Path = (Config->Sysroot + S).toStringRef(PathData);
    if (sys::fs::exists(Path)) {
      Driver->addFile(Saver.save(Path), /*WithLOption=*/false);
      return;
    }
  }

  if (sys::path::is_absolute(S)) {
    Driver->addFile(S, /*WithLOption=*/false);
  } else if (S.startswith("=")) {
    if (Config->Sysroot.empty())
      Driver->addFile(S.substr(1), /*WithLOption=*/false);
    else
      Driver->addFile(Saver.save(Config->Sysroot + "/" + S.substr(1)),
                      /*WithLOption=*/false);
  } else if (S.startswith("-l")) {
    Driver->addLibrary(S.substr(2));
  } else if (sys::fs::exists(S)) {
    Driver->addFile(S, /*WithLOption=*/false);
  } else {
    if (Optional<std::string> Path = findFromSearchPaths(S))
      Driver->addFile(Saver.save(*Path), /*WithLOption=*/true);
    else
      setError("unable to find " + S);
  }
}

void ScriptParser::readAsNeeded() {
  expect("(");
  bool Orig = Config->AsNeeded;
  Config->AsNeeded = true;
  while (!Error && !consume(")"))
    addFile(unquote(next()));
  Config->AsNeeded = Orig;
}

void ScriptParser::readEntry() {
  // -e <symbol> takes predecence over ENTRY(<symbol>).
  expect("(");
  StringRef Tok = next();
  if (Config->Entry.empty())
    Config->Entry = Tok;
  expect(")");
}

void ScriptParser::readExtern() {
  expect("(");
  while (!Error && !consume(")"))
    Config->Undefined.push_back(next());
}

void ScriptParser::readGroup() {
  expect("(");
  while (!Error && !consume(")")) {
    if (consume("AS_NEEDED"))
      readAsNeeded();
    else
      addFile(unquote(next()));
  }
}

void ScriptParser::readInclude() {
  StringRef Tok = unquote(next());

  // https://sourceware.org/binutils/docs/ld/File-Commands.html:
  // The file will be searched for in the current directory, and in any
  // directory specified with the -L option.
  if (sys::fs::exists(Tok)) {
    if (Optional<MemoryBufferRef> MB = readFile(Tok))
      tokenize(*MB);
    return;
  }
  if (Optional<std::string> Path = findFromSearchPaths(Tok)) {
    if (Optional<MemoryBufferRef> MB = readFile(*Path))
      tokenize(*MB);
    return;
  }
  setError("cannot open " + Tok);
}

void ScriptParser::readOutput() {
  // -o <file> takes predecence over OUTPUT(<file>).
  expect("(");
  StringRef Tok = next();
  if (Config->OutputFile.empty())
    Config->OutputFile = unquote(Tok);
  expect(")");
}

void ScriptParser::readOutputArch() {
  // OUTPUT_ARCH is ignored for now.
  expect("(");
  while (!Error && !consume(")"))
    skip();
}

void ScriptParser::readOutputFormat() {
  // Error checking only for now.
  expect("(");
  skip();
  if (consume(")"))
    return;
  expect(",");
  skip();
  expect(",");
  skip();
  expect(")");
}

void ScriptParser::readPhdrs() {
  expect("{");
  while (!Error && !consume("}")) {
    Script->Opt.PhdrsCommands.push_back(
        {next(), PT_NULL, false, false, UINT_MAX, nullptr});

    PhdrsCommand &PhdrCmd = Script->Opt.PhdrsCommands.back();
    PhdrCmd.Type = readPhdrType();

    while (!Error && !consume(";")) {
      if (consume("FILEHDR"))
        PhdrCmd.HasFilehdr = true;
      else if (consume("PHDRS"))
        PhdrCmd.HasPhdrs = true;
      else if (consume("AT"))
        PhdrCmd.LMAExpr = readParenExpr();
      else if (consume("FLAGS"))
        PhdrCmd.Flags = readParenExpr()().getValue();
      else
        setError("unexpected header attribute: " + next());
    }
  }
}

void ScriptParser::readSearchDir() {
  expect("(");
  StringRef Tok = next();
  if (!Config->Nostdlib)
    Config->SearchPaths.push_back(unquote(Tok));
  expect(")");
}

void ScriptParser::readSections() {
  Script->Opt.HasSections = true;

  // -no-rosegment is used to avoid placing read only non-executable sections in
  // their own segment. We do the same if SECTIONS command is present in linker
  // script. See comment for computeFlags().
  Config->SingleRoRx = true;

  expect("{");
  while (!Error && !consume("}")) {
    StringRef Tok = next();
    BaseCommand *Cmd = readProvideOrAssignment(Tok);
    if (!Cmd) {
      if (Tok == "ASSERT")
        Cmd = readAssert();
      else
        Cmd = readOutputSectionDescription(Tok);
    }
    Script->Opt.Commands.push_back(Cmd);
  }
}

static int precedence(StringRef Op) {
  return StringSwitch<int>(Op)
      .Cases("*", "/", 5)
      .Cases("+", "-", 4)
      .Cases("<<", ">>", 3)
      .Cases("<", "<=", ">", ">=", "==", "!=", 2)
      .Cases("&", "|", 1)
      .Default(-1);
}

StringMatcher ScriptParser::readFilePatterns() {
  std::vector<StringRef> V;
  while (!Error && !consume(")"))
    V.push_back(next());
  return StringMatcher(V);
}

SortSectionPolicy ScriptParser::readSortKind() {
  if (consume("SORT") || consume("SORT_BY_NAME"))
    return SortSectionPolicy::Name;
  if (consume("SORT_BY_ALIGNMENT"))
    return SortSectionPolicy::Alignment;
  if (consume("SORT_BY_INIT_PRIORITY"))
    return SortSectionPolicy::Priority;
  if (consume("SORT_NONE"))
    return SortSectionPolicy::None;
  return SortSectionPolicy::Default;
}

// Reads SECTIONS command contents in the following form:
//
// <contents> ::= <elem>*
// <elem>     ::= <exclude>? <glob-pattern>
// <exclude>  ::= "EXCLUDE_FILE" "(" <glob-pattern>+ ")"
//
// For example,
//
// *(.foo EXCLUDE_FILE (a.o) .bar EXCLUDE_FILE (b.o) .baz)
//
// is parsed as ".foo", ".bar" with "a.o", and ".baz" with "b.o".
// The semantics of that is section .foo in any file, section .bar in
// any file but a.o, and section .baz in any file but b.o.
std::vector<SectionPattern> ScriptParser::readInputSectionsList() {
  std::vector<SectionPattern> Ret;
  while (!Error && peek() != ")") {
    StringMatcher ExcludeFilePat;
    if (consume("EXCLUDE_FILE")) {
      expect("(");
      ExcludeFilePat = readFilePatterns();
    }

    std::vector<StringRef> V;
    while (!Error && peek() != ")" && peek() != "EXCLUDE_FILE")
      V.push_back(next());

    if (!V.empty())
      Ret.push_back({std::move(ExcludeFilePat), StringMatcher(V)});
    else
      setError("section pattern is expected");
  }
  return Ret;
}

// Reads contents of "SECTIONS" directive. That directive contains a
// list of glob patterns for input sections. The grammar is as follows.
//
// <patterns> ::= <section-list>
//              | <sort> "(" <section-list> ")"
//              | <sort> "(" <sort> "(" <section-list> ")" ")"
//
// <sort>     ::= "SORT" | "SORT_BY_NAME" | "SORT_BY_ALIGNMENT"
//              | "SORT_BY_INIT_PRIORITY" | "SORT_NONE"
//
// <section-list> is parsed by readInputSectionsList().
InputSectionDescription *
ScriptParser::readInputSectionRules(StringRef FilePattern) {
  auto *Cmd = make<InputSectionDescription>(FilePattern);
  expect("(");

  while (!Error && !consume(")")) {
    SortSectionPolicy Outer = readSortKind();
    SortSectionPolicy Inner = SortSectionPolicy::Default;
    std::vector<SectionPattern> V;
    if (Outer != SortSectionPolicy::Default) {
      expect("(");
      Inner = readSortKind();
      if (Inner != SortSectionPolicy::Default) {
        expect("(");
        V = readInputSectionsList();
        expect(")");
      } else {
        V = readInputSectionsList();
      }
      expect(")");
    } else {
      V = readInputSectionsList();
    }

    for (SectionPattern &Pat : V) {
      Pat.SortInner = Inner;
      Pat.SortOuter = Outer;
    }

    std::move(V.begin(), V.end(), std::back_inserter(Cmd->SectionPatterns));
  }
  return Cmd;
}

InputSectionDescription *
ScriptParser::readInputSectionDescription(StringRef Tok) {
  // Input section wildcard can be surrounded by KEEP.
  // https://sourceware.org/binutils/docs/ld/Input-Section-Keep.html#Input-Section-Keep
  if (Tok == "KEEP") {
    expect("(");
    StringRef FilePattern = next();
    InputSectionDescription *Cmd = readInputSectionRules(FilePattern);
    expect(")");
    Script->Opt.KeptSections.push_back(Cmd);
    return Cmd;
  }
  return readInputSectionRules(Tok);
}

void ScriptParser::readSort() {
  expect("(");
  expect("CONSTRUCTORS");
  expect(")");
}

AssertCommand *ScriptParser::readAssert() {
  return make<AssertCommand>(readAssertExpr());
}

Expr ScriptParser::readAssertExpr() {
  expect("(");
  Expr E = readExpr();
  expect(",");
  StringRef Msg = unquote(next());
  expect(")");

  return [=] {
    if (!E().getValue())
      error(Msg);
    return Script->getDot();
  };
}

// Reads a FILL(expr) command. We handle the FILL command as an
// alias for =fillexp section attribute, which is different from
// what GNU linkers do.
// https://sourceware.org/binutils/docs/ld/Output-Section-Data.html
uint32_t ScriptParser::readFill() {
  expect("(");
  uint32_t V = parseFill(next());
  expect(")");
  return V;
}

// Reads an expression and/or the special directive "(NOLOAD)" for an
// output section definition.
//
// An output section name can be followed by an address expression
// and/or by "(NOLOAD)". This grammar is not LL(1) because "(" can be
// interpreted as either the beginning of some expression or "(NOLOAD)".
//
// https://sourceware.org/binutils/docs/ld/Output-Section-Address.html
// https://sourceware.org/binutils/docs/ld/Output-Section-Type.html
void ScriptParser::readSectionAddressType(OutputSectionCommand *Cmd) {
  if (consume("(")) {
    if (consume("NOLOAD")) {
      expect(")");
      Cmd->Noload = true;
      return;
    }
    Cmd->AddrExpr = readExpr();
    expect(")");
  } else {
    Cmd->AddrExpr = readExpr();
  }

  if (consume("(")) {
    expect("NOLOAD");
    expect(")");
    Cmd->Noload = true;
  }
}

OutputSectionCommand *
ScriptParser::readOutputSectionDescription(StringRef OutSec) {
  OutputSectionCommand *Cmd =
      Script->createOutputSectionCommand(OutSec, getCurrentLocation());

  if (peek() != ":")
    readSectionAddressType(Cmd);
  expect(":");

  if (consume("AT"))
    Cmd->LMAExpr = readParenExpr();
  if (consume("ALIGN"))
    Cmd->AlignExpr = readParenExpr();
  if (consume("SUBALIGN"))
    Cmd->SubalignExpr = readParenExpr();

  // Parse constraints.
  if (consume("ONLY_IF_RO"))
    Cmd->Constraint = ConstraintKind::ReadOnly;
  if (consume("ONLY_IF_RW"))
    Cmd->Constraint = ConstraintKind::ReadWrite;
  expect("{");

  while (!Error && !consume("}")) {
    StringRef Tok = next();
    if (Tok == ";") {
      // Empty commands are allowed. Do nothing here.
    } else if (SymbolAssignment *Assign = readProvideOrAssignment(Tok)) {
      Cmd->Commands.push_back(Assign);
    } else if (BytesDataCommand *Data = readBytesDataCommand(Tok)) {
      Cmd->Commands.push_back(Data);
    } else if (Tok == "ASSERT") {
      Cmd->Commands.push_back(readAssert());
      expect(";");
    } else if (Tok == "CONSTRUCTORS") {
      // CONSTRUCTORS is a keyword to make the linker recognize C++ ctors/dtors
      // by name. This is for very old file formats such as ECOFF/XCOFF.
      // For ELF, we should ignore.
    } else if (Tok == "FILL") {
      Cmd->Filler = readFill();
    } else if (Tok == "SORT") {
      readSort();
    } else if (peek() == "(") {
      Cmd->Commands.push_back(readInputSectionDescription(Tok));
    } else {
      setError("unknown command " + Tok);
    }
  }

  if (consume(">"))
    Cmd->MemoryRegionName = next();

  Cmd->Phdrs = readOutputSectionPhdrs();

  if (consume("="))
    Cmd->Filler = parseFill(next());
  else if (peek().startswith("="))
    Cmd->Filler = parseFill(next().drop_front());

  // Consume optional comma following output section command.
  consume(",");

  return Cmd;
}

// Parses a given string as a octal/decimal/hexadecimal number and
// returns it as a big-endian number. Used for `=<fillexp>`.
// https://sourceware.org/binutils/docs/ld/Output-Section-Fill.html
//
// When reading a hexstring, ld.bfd handles it as a blob of arbitrary
// size, while ld.gold always handles it as a 32-bit big-endian number.
// We are compatible with ld.gold because it's easier to implement.
uint32_t ScriptParser::parseFill(StringRef Tok) {
  uint32_t V = 0;
  if (!to_integer(Tok, V))
    setError("invalid filler expression: " + Tok);

  uint32_t Buf;
  write32be(&Buf, V);
  return Buf;
}

SymbolAssignment *ScriptParser::readProvideHidden(bool Provide, bool Hidden) {
  expect("(");
  SymbolAssignment *Cmd = readAssignment(next());
  Cmd->Provide = Provide;
  Cmd->Hidden = Hidden;
  expect(")");
  expect(";");
  return Cmd;
}

SymbolAssignment *ScriptParser::readProvideOrAssignment(StringRef Tok) {
  SymbolAssignment *Cmd = nullptr;
  if (peek() == "=" || peek() == "+=") {
    Cmd = readAssignment(Tok);
    expect(";");
  } else if (Tok == "PROVIDE") {
    Cmd = readProvideHidden(true, false);
  } else if (Tok == "HIDDEN") {
    Cmd = readProvideHidden(false, true);
  } else if (Tok == "PROVIDE_HIDDEN") {
    Cmd = readProvideHidden(true, true);
  }
  return Cmd;
}

SymbolAssignment *ScriptParser::readAssignment(StringRef Name) {
  StringRef Op = next();
  assert(Op == "=" || Op == "+=");
  Expr E = readExpr();
  if (Op == "+=") {
    std::string Loc = getCurrentLocation();
    E = [=] { return add(Script->getSymbolValue(Loc, Name), E()); };
  }
  return make<SymbolAssignment>(Name, E, getCurrentLocation());
}

// This is an operator-precedence parser to parse a linker
// script expression.
Expr ScriptParser::readExpr() {
  // Our lexer is context-aware. Set the in-expression bit so that
  // they apply different tokenization rules.
  bool Orig = InExpr;
  InExpr = true;
  Expr E = readExpr1(readPrimary(), 0);
  InExpr = Orig;
  return E;
}

static Expr combine(StringRef Op, Expr L, Expr R) {
  if (Op == "+")
    return [=] { return add(L(), R()); };
  if (Op == "-")
    return [=] { return sub(L(), R()); };
  if (Op == "*")
    return [=] { return mul(L(), R()); };
  if (Op == "/")
    return [=] { return div(L(), R()); };
  if (Op == "<<")
    return [=] { return L().getValue() << R().getValue(); };
  if (Op == ">>")
    return [=] { return L().getValue() >> R().getValue(); };
  if (Op == "<")
    return [=] { return L().getValue() < R().getValue(); };
  if (Op == ">")
    return [=] { return L().getValue() > R().getValue(); };
  if (Op == ">=")
    return [=] { return L().getValue() >= R().getValue(); };
  if (Op == "<=")
    return [=] { return L().getValue() <= R().getValue(); };
  if (Op == "==")
    return [=] { return L().getValue() == R().getValue(); };
  if (Op == "!=")
    return [=] { return L().getValue() != R().getValue(); };
  if (Op == "&")
    return [=] { return bitAnd(L(), R()); };
  if (Op == "|")
    return [=] { return bitOr(L(), R()); };
  llvm_unreachable("invalid operator");
}

// This is a part of the operator-precedence parser. This function
// assumes that the remaining token stream starts with an operator.
Expr ScriptParser::readExpr1(Expr Lhs, int MinPrec) {
  while (!atEOF() && !Error) {
    // Read an operator and an expression.
    if (consume("?"))
      return readTernary(Lhs);
    StringRef Op1 = peek();
    if (precedence(Op1) < MinPrec)
      break;
    skip();
    Expr Rhs = readPrimary();

    // Evaluate the remaining part of the expression first if the
    // next operator has greater precedence than the previous one.
    // For example, if we have read "+" and "3", and if the next
    // operator is "*", then we'll evaluate 3 * ... part first.
    while (!atEOF()) {
      StringRef Op2 = peek();
      if (precedence(Op2) <= precedence(Op1))
        break;
      Rhs = readExpr1(Rhs, precedence(Op2));
    }

    Lhs = combine(Op1, Lhs, Rhs);
  }
  return Lhs;
}

uint64_t static getConstant(StringRef S) {
  if (S == "COMMONPAGESIZE")
    return Target->PageSize;
  if (S == "MAXPAGESIZE")
    return Config->MaxPageSize;
  error("unknown constant: " + S);
  return 0;
}

// Parses Tok as an integer. It recognizes hexadecimal (prefixed with
// "0x" or suffixed with "H") and decimal numbers. Decimal numbers may
// have "K" (Ki) or "M" (Mi) suffixes.
static Optional<uint64_t> parseInt(StringRef Tok) {
  // Negative number
  if (Tok.startswith("-")) {
    if (Optional<uint64_t> Val = parseInt(Tok.substr(1)))
      return -*Val;
    return None;
  }

  // Hexadecimal
  uint64_t Val;
  if (Tok.startswith_lower("0x") && to_integer(Tok.substr(2), Val, 16))
    return Val;
  if (Tok.endswith_lower("H") && to_integer(Tok.drop_back(), Val, 16))
    return Val;

  // Decimal
  if (Tok.endswith_lower("K")) {
    if (!to_integer(Tok.drop_back(), Val, 10))
      return None;
    return Val * 1024;
  }
  if (Tok.endswith_lower("M")) {
    if (!to_integer(Tok.drop_back(), Val, 10))
      return None;
    return Val * 1024 * 1024;
  }
  if (!to_integer(Tok, Val, 10))
    return None;
  return Val;
}

BytesDataCommand *ScriptParser::readBytesDataCommand(StringRef Tok) {
  int Size = StringSwitch<int>(Tok)
                 .Case("BYTE", 1)
                 .Case("SHORT", 2)
                 .Case("LONG", 4)
                 .Case("QUAD", 8)
                 .Default(-1);
  if (Size == -1)
    return nullptr;

  return make<BytesDataCommand>(readParenExpr(), Size);
}

StringRef ScriptParser::readParenLiteral() {
  expect("(");
  StringRef Tok = next();
  expect(")");
  return Tok;
}

OutputSection *ScriptParser::checkSection(OutputSectionCommand *Cmd,
                                          StringRef Location) {
  if (Cmd->Location.empty() && Script->ErrorOnMissingSection)
    error(Location + ": undefined section " + Cmd->Name);
  if (Cmd->Sec)
    return Cmd->Sec;
  static OutputSection Dummy("", 0, 0);
  return &Dummy;
}

Expr ScriptParser::readPrimary() {
  if (peek() == "(")
    return readParenExpr();

  if (consume("~")) {
    Expr E = readPrimary();
    return [=] { return ~E().getValue(); };
  }
  if (consume("-")) {
    Expr E = readPrimary();
    return [=] { return -E().getValue(); };
  }

  StringRef Tok = next();
  std::string Location = getCurrentLocation();

  // Built-in functions are parsed here.
  // https://sourceware.org/binutils/docs/ld/Builtin-Functions.html.
  if (Tok == "ABSOLUTE") {
    Expr Inner = readParenExpr();
    return [=] {
      ExprValue I = Inner();
      I.ForceAbsolute = true;
      return I;
    };
  }
  if (Tok == "ADDR") {
    StringRef Name = readParenLiteral();
    OutputSectionCommand *Cmd = Script->getOrCreateOutputSectionCommand(Name);
    return [=]() -> ExprValue {
      return {checkSection(Cmd, Location), 0, Location};
    };
  }
  if (Tok == "ALIGN") {
    expect("(");
    Expr E = readExpr();
    if (consume(")"))
      return [=] { return alignTo(Script->getDot(), E().getValue()); };
    expect(",");
    Expr E2 = readExpr();
    expect(")");
    return [=] {
      ExprValue V = E();
      V.Alignment = E2().getValue();
      return V;
    };
  }
  if (Tok == "ALIGNOF") {
    StringRef Name = readParenLiteral();
    OutputSectionCommand *Cmd = Script->getOrCreateOutputSectionCommand(Name);
    return [=] { return checkSection(Cmd, Location)->Alignment; };
  }
  if (Tok == "ASSERT")
    return readAssertExpr();
  if (Tok == "CONSTANT") {
    StringRef Name = readParenLiteral();
    return [=] { return getConstant(Name); };
  }
  if (Tok == "DATA_SEGMENT_ALIGN") {
    expect("(");
    Expr E = readExpr();
    expect(",");
    readExpr();
    expect(")");
    return [=] { return alignTo(Script->getDot(), E().getValue()); };
  }
  if (Tok == "DATA_SEGMENT_END") {
    expect("(");
    expect(".");
    expect(")");
    return [] { return Script->getDot(); };
  }
  if (Tok == "DATA_SEGMENT_RELRO_END") {
    // GNU linkers implements more complicated logic to handle
    // DATA_SEGMENT_RELRO_END. We instead ignore the arguments and
    // just align to the next page boundary for simplicity.
    expect("(");
    readExpr();
    expect(",");
    readExpr();
    expect(")");
    return [] { return alignTo(Script->getDot(), Target->PageSize); };
  }
  if (Tok == "DEFINED") {
    StringRef Name = readParenLiteral();
    return [=] { return Script->isDefined(Name) ? 1 : 0; };
  }
  if (Tok == "LENGTH") {
    StringRef Name = readParenLiteral();
    if (Script->Opt.MemoryRegions.count(Name) == 0)
      setError("memory region not defined: " + Name);
    return [=] { return Script->Opt.MemoryRegions[Name].Length; };
  }
  if (Tok == "LOADADDR") {
    StringRef Name = readParenLiteral();
    OutputSectionCommand *Cmd = Script->getOrCreateOutputSectionCommand(Name);
    return [=] { return checkSection(Cmd, Location)->getLMA(); };
  }
  if (Tok == "ORIGIN") {
    StringRef Name = readParenLiteral();
    if (Script->Opt.MemoryRegions.count(Name) == 0)
      setError("memory region not defined: " + Name);
    return [=] { return Script->Opt.MemoryRegions[Name].Origin; };
  }
  if (Tok == "SEGMENT_START") {
    expect("(");
    skip();
    expect(",");
    Expr E = readExpr();
    expect(")");
    return [=] { return E(); };
  }
  if (Tok == "SIZEOF") {
    StringRef Name = readParenLiteral();
    OutputSectionCommand *Cmd = Script->getOrCreateOutputSectionCommand(Name);
    // Linker script does not create an output section if its content is empty.
    // We want to allow SIZEOF(.foo) where .foo is a section which happened to
    // be empty.
    return [=] { return Cmd->Sec ? Cmd->Sec->Size : 0; };
  }
  if (Tok == "SIZEOF_HEADERS")
    return [=] { return elf::getHeaderSize(); };

  // Tok is the dot.
  if (Tok == ".")
    return [=] { return Script->getSymbolValue(Location, Tok); };

  // Tok is a literal number.
  if (Optional<uint64_t> Val = parseInt(Tok))
    return [=] { return *Val; };

  // Tok is a symbol name.
  if (!isValidCIdentifier(Tok))
    setError("malformed number: " + Tok);
  Script->Opt.ReferencedSymbols.push_back(Tok);
  return [=] { return Script->getSymbolValue(Location, Tok); };
}

Expr ScriptParser::readTernary(Expr Cond) {
  Expr L = readExpr();
  expect(":");
  Expr R = readExpr();
  return [=] { return Cond().getValue() ? L() : R(); };
}

Expr ScriptParser::readParenExpr() {
  expect("(");
  Expr E = readExpr();
  expect(")");
  return E;
}

std::vector<StringRef> ScriptParser::readOutputSectionPhdrs() {
  std::vector<StringRef> Phdrs;
  while (!Error && peek().startswith(":")) {
    StringRef Tok = next();
    Phdrs.push_back((Tok.size() == 1) ? next() : Tok.substr(1));
  }
  return Phdrs;
}

// Read a program header type name. The next token must be a
// name of a program header type or a constant (e.g. "0x3").
unsigned ScriptParser::readPhdrType() {
  StringRef Tok = next();
  if (Optional<uint64_t> Val = parseInt(Tok))
    return *Val;

  unsigned Ret = StringSwitch<unsigned>(Tok)
                     .Case("PT_NULL", PT_NULL)
                     .Case("PT_LOAD", PT_LOAD)
                     .Case("PT_DYNAMIC", PT_DYNAMIC)
                     .Case("PT_INTERP", PT_INTERP)
                     .Case("PT_NOTE", PT_NOTE)
                     .Case("PT_SHLIB", PT_SHLIB)
                     .Case("PT_PHDR", PT_PHDR)
                     .Case("PT_TLS", PT_TLS)
                     .Case("PT_GNU_EH_FRAME", PT_GNU_EH_FRAME)
                     .Case("PT_GNU_STACK", PT_GNU_STACK)
                     .Case("PT_GNU_RELRO", PT_GNU_RELRO)
                     .Case("PT_OPENBSD_RANDOMIZE", PT_OPENBSD_RANDOMIZE)
                     .Case("PT_OPENBSD_WXNEEDED", PT_OPENBSD_WXNEEDED)
                     .Case("PT_OPENBSD_BOOTDATA", PT_OPENBSD_BOOTDATA)
                     .Default(-1);

  if (Ret == (unsigned)-1) {
    setError("invalid program header type: " + Tok);
    return PT_NULL;
  }
  return Ret;
}

// Reads an anonymous version declaration.
void ScriptParser::readAnonymousDeclaration() {
  std::vector<SymbolVersion> Locals;
  std::vector<SymbolVersion> Globals;
  std::tie(Locals, Globals) = readSymbols();

  for (SymbolVersion V : Locals) {
    if (V.Name == "*")
      Config->DefaultSymbolVersion = VER_NDX_LOCAL;
    else
      Config->VersionScriptLocals.push_back(V);
  }

  for (SymbolVersion V : Globals)
    Config->VersionScriptGlobals.push_back(V);

  expect(";");
}

// Reads a non-anonymous version definition,
// e.g. "VerStr { global: foo; bar; local: *; };".
void ScriptParser::readVersionDeclaration(StringRef VerStr) {
  // Read a symbol list.
  std::vector<SymbolVersion> Locals;
  std::vector<SymbolVersion> Globals;
  std::tie(Locals, Globals) = readSymbols();

  for (SymbolVersion V : Locals) {
    if (V.Name == "*")
      Config->DefaultSymbolVersion = VER_NDX_LOCAL;
    else
      Config->VersionScriptLocals.push_back(V);
  }

  // Create a new version definition and add that to the global symbols.
  VersionDefinition Ver;
  Ver.Name = VerStr;
  Ver.Globals = Globals;

  // User-defined version number starts from 2 because 0 and 1 are
  // reserved for VER_NDX_LOCAL and VER_NDX_GLOBAL, respectively.
  Ver.Id = Config->VersionDefinitions.size() + 2;
  Config->VersionDefinitions.push_back(Ver);

  // Each version may have a parent version. For example, "Ver2"
  // defined as "Ver2 { global: foo; local: *; } Ver1;" has "Ver1"
  // as a parent. This version hierarchy is, probably against your
  // instinct, purely for hint; the runtime doesn't care about it
  // at all. In LLD, we simply ignore it.
  if (peek() != ";")
    skip();
  expect(";");
}

static bool hasWildcard(StringRef S) {
  return S.find_first_of("?*[") != StringRef::npos;
}

// Reads a list of symbols, e.g. "{ global: foo; bar; local: *; };".
std::pair<std::vector<SymbolVersion>, std::vector<SymbolVersion>>
ScriptParser::readSymbols() {
  std::vector<SymbolVersion> Locals;
  std::vector<SymbolVersion> Globals;
  std::vector<SymbolVersion> *V = &Globals;

  while (!Error) {
    if (consume("}"))
      break;
    if (consumeLabel("local")) {
      V = &Locals;
      continue;
    }
    if (consumeLabel("global")) {
      V = &Globals;
      continue;
    }

    if (consume("extern")) {
      std::vector<SymbolVersion> Ext = readVersionExtern();
      V->insert(V->end(), Ext.begin(), Ext.end());
    } else {
      StringRef Tok = next();
      V->push_back({unquote(Tok), false, hasWildcard(Tok)});
    }
    expect(";");
  }
  return {Locals, Globals};
}

// Reads an "extern C++" directive, e.g.,
// "extern "C++" { ns::*; "f(int, double)"; };"
std::vector<SymbolVersion> ScriptParser::readVersionExtern() {
  StringRef Tok = next();
  bool IsCXX = Tok == "\"C++\"";
  if (!IsCXX && Tok != "\"C\"")
    setError("Unknown language");
  expect("{");

  std::vector<SymbolVersion> Ret;
  while (!Error && peek() != "}") {
    StringRef Tok = next();
    bool HasWildcard = !Tok.startswith("\"") && hasWildcard(Tok);
    Ret.push_back({unquote(Tok), IsCXX, HasWildcard});
    expect(";");
  }

  expect("}");
  return Ret;
}

uint64_t ScriptParser::readMemoryAssignment(StringRef S1, StringRef S2,
                                            StringRef S3) {
  if (!consume(S1) && !consume(S2) && !consume(S3)) {
    setError("expected one of: " + S1 + ", " + S2 + ", or " + S3);
    return 0;
  }
  expect("=");
  return readExpr()().getValue();
}

// Parse the MEMORY command as specified in:
// https://sourceware.org/binutils/docs/ld/MEMORY.html
//
// MEMORY { name [(attr)] : ORIGIN = origin, LENGTH = len ... }
void ScriptParser::readMemory() {
  expect("{");
  while (!Error && !consume("}")) {
    StringRef Name = next();

    uint32_t Flags = 0;
    uint32_t NegFlags = 0;
    if (consume("(")) {
      std::tie(Flags, NegFlags) = readMemoryAttributes();
      expect(")");
    }
    expect(":");

    uint64_t Origin = readMemoryAssignment("ORIGIN", "org", "o");
    expect(",");
    uint64_t Length = readMemoryAssignment("LENGTH", "len", "l");

    // Add the memory region to the region map (if it doesn't already exist).
    auto It = Script->Opt.MemoryRegions.find(Name);
    if (It != Script->Opt.MemoryRegions.end())
      setError("region '" + Name + "' already defined");
    else
      Script->Opt.MemoryRegions[Name] = {Name, Origin, Length, Flags, NegFlags};
  }
}

// This function parses the attributes used to match against section
// flags when placing output sections in a memory region. These flags
// are only used when an explicit memory region name is not used.
std::pair<uint32_t, uint32_t> ScriptParser::readMemoryAttributes() {
  uint32_t Flags = 0;
  uint32_t NegFlags = 0;
  bool Invert = false;

  for (char C : next().lower()) {
    uint32_t Flag = 0;
    if (C == '!')
      Invert = !Invert;
    else if (C == 'w')
      Flag = SHF_WRITE;
    else if (C == 'x')
      Flag = SHF_EXECINSTR;
    else if (C == 'a')
      Flag = SHF_ALLOC;
    else if (C != 'r')
      setError("invalid memory region attribute");

    if (Invert)
      NegFlags |= Flag;
    else
      Flags |= Flag;
  }
  return {Flags, NegFlags};
}

void elf::readLinkerScript(MemoryBufferRef MB) {
  ScriptParser(MB).readLinkerScript();
}

void elf::readVersionScript(MemoryBufferRef MB) {
  ScriptParser(MB).readVersionScript();
}

void elf::readDynamicList(MemoryBufferRef MB) {
  ScriptParser(MB).readDynamicList();
}
