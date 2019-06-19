//===- unittest/AST/ASTImporterTest.cpp - AST node import test ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Tests for the correct import of AST nodes from one AST context to another.
//
//===----------------------------------------------------------------------===//

#include "MatchVerifier.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTImporter.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"

#include "DeclMatcher.h"
#include "Language.h"
#include "gmock/gmock.h"
#include "llvm/ADT/StringMap.h"

namespace clang {
namespace ast_matchers {

using internal::Matcher;
using internal::BindableMatcher;
using llvm::StringMap;

// Creates a virtual file and assigns that to the context of given AST. If the
// file already exists then the file will not be created again as a duplicate.
static void
createVirtualFileIfNeeded(ASTUnit *ToAST, StringRef FileName,
                          std::unique_ptr<llvm::MemoryBuffer> &&Buffer) {
  assert(ToAST);
  ASTContext &ToCtx = ToAST->getASTContext();
  auto *OFS = static_cast<vfs::OverlayFileSystem *>(
      ToCtx.getSourceManager().getFileManager().getVirtualFileSystem().get());
  auto *MFS =
      static_cast<vfs::InMemoryFileSystem *>(OFS->overlays_begin()->get());
  MFS->addFile(FileName, 0, std::move(Buffer));
}

static void createVirtualFileIfNeeded(ASTUnit *ToAST, StringRef FileName,
                                      StringRef Code) {
  return createVirtualFileIfNeeded(ToAST, FileName,
                                   llvm::MemoryBuffer::getMemBuffer(Code));
}

const StringRef DeclToImportID = "declToImport";
const StringRef DeclToVerifyID = "declToVerify";

// Common base for the different families of ASTImporter tests that are
// parameterized on the compiler options which may result a different AST. E.g.
// -fms-compatibility or -fdelayed-template-parsing.
struct ParameterizedTestsFixture : ::testing::TestWithParam<ArgVector> {

  // Returns the argument vector used for a specific language option, this set
  // can be tweaked by the test parameters.
  ArgVector getArgVectorForLanguage(Language Lang) const {
    ArgVector Args = getBasicRunOptionsForLanguage(Lang);
    ArgVector ExtraArgs = GetParam();
    for (const auto &Arg : ExtraArgs) {
      Args.push_back(Arg);
    }
    return Args;
  }

};

// Base class for those tests which use the family of `testImport` functions.
class TestImportBase : public ParameterizedTestsFixture {

  template <typename NodeType>
  NodeType importNode(ASTUnit *From, ASTUnit *To, ASTImporter &Importer,
                      NodeType Node) {
    ASTContext &ToCtx = To->getASTContext();

    // Add 'From' file to virtual file system so importer can 'find' it
    // while importing SourceLocations. It is safe to add same file multiple
    // times - it just isn't replaced.
    StringRef FromFileName = From->getMainFileName();
    createVirtualFileIfNeeded(To, FromFileName,
                              From->getBufferForFile(FromFileName));

    auto Imported = Importer.Import(Node);

    // This should dump source locations and assert if some source locations
    // were not imported.
    SmallString<1024> ImportChecker;
    llvm::raw_svector_ostream ToNothing(ImportChecker);
    ToCtx.getTranslationUnitDecl()->print(ToNothing);

    // This traverses the AST to catch certain bugs like poorly or not
    // implemented subtrees.
    Imported->dump(ToNothing);

    return Imported;
  }

  template <typename NodeType>
  testing::AssertionResult
  testImport(const std::string &FromCode, const ArgVector &FromArgs,
             const std::string &ToCode, const ArgVector &ToArgs,
             MatchVerifier<NodeType> &Verifier,
             const BindableMatcher<NodeType> &SearchMatcher,
             const BindableMatcher<NodeType> &VerificationMatcher) {
    const char *const InputFileName = "input.cc";
    const char *const OutputFileName = "output.cc";

    std::unique_ptr<ASTUnit> FromAST = tooling::buildASTFromCodeWithArgs(
                                 FromCode, FromArgs, InputFileName),
                             ToAST = tooling::buildASTFromCodeWithArgs(
                                 ToCode, ToArgs, OutputFileName);

    ASTContext &FromCtx = FromAST->getASTContext(),
               &ToCtx = ToAST->getASTContext();

    ASTImporter Importer(ToCtx, ToAST->getFileManager(), FromCtx,
                         FromAST->getFileManager(), false);

    auto FoundNodes = match(SearchMatcher, FromCtx);
    if (FoundNodes.size() != 1)
      return testing::AssertionFailure()
             << "Multiple potential nodes were found!";

    auto ToImport = selectFirst<NodeType>(DeclToImportID, FoundNodes);
    if (!ToImport)
      return testing::AssertionFailure() << "Node type mismatch!";

    // Sanity check: the node being imported should match in the same way as
    // the result node.
    BindableMatcher<NodeType> WrapperMatcher(VerificationMatcher);
    EXPECT_TRUE(Verifier.match(ToImport, WrapperMatcher));

    auto Imported = importNode(FromAST.get(), ToAST.get(), Importer, ToImport);
    if (!Imported)
      return testing::AssertionFailure() << "Import failed, nullptr returned!";

    return Verifier.match(Imported, WrapperMatcher);
  }

  template <typename NodeType>
  testing::AssertionResult
  testImport(const std::string &FromCode, const ArgVector &FromArgs,
             const std::string &ToCode, const ArgVector &ToArgs,
             MatchVerifier<NodeType> &Verifier,
             const BindableMatcher<NodeType> &VerificationMatcher) {
    return testImport(
        FromCode, FromArgs, ToCode, ToArgs, Verifier,
        translationUnitDecl(
            has(namedDecl(hasName(DeclToImportID)).bind(DeclToImportID))),
        VerificationMatcher);
  }

public:

  /// Test how AST node named "declToImport" located in the translation unit
  /// of "FromCode" virtual file is imported to "ToCode" virtual file.
  /// The verification is done by running AMatcher over the imported node.
  template <typename NodeType, typename MatcherType>
  void testImport(const std::string &FromCode, Language FromLang,
                  const std::string &ToCode, Language ToLang,
                  MatchVerifier<NodeType> &Verifier,
                  const MatcherType &AMatcher) {
    ArgVector FromArgs = getArgVectorForLanguage(FromLang),
              ToArgs = getArgVectorForLanguage(ToLang);
    EXPECT_TRUE(
        testImport(FromCode, FromArgs, ToCode, ToArgs, Verifier, AMatcher));
  }

  struct ImportAction {
    StringRef FromFilename;
    StringRef ToFilename;
    // FIXME: Generalize this to support other node kinds.
    BindableMatcher<Decl> ImportPredicate;

    ImportAction(StringRef FromFilename, StringRef ToFilename,
                 DeclarationMatcher ImportPredicate)
        : FromFilename(FromFilename), ToFilename(ToFilename),
          ImportPredicate(ImportPredicate) {}

    ImportAction(StringRef FromFilename, StringRef ToFilename,
                 const std::string &DeclName)
        : FromFilename(FromFilename), ToFilename(ToFilename),
          ImportPredicate(namedDecl(hasName(DeclName))) {}
  };

  using SingleASTUnit = std::unique_ptr<ASTUnit>;
  using AllASTUnits = StringMap<SingleASTUnit>;

  struct CodeEntry {
    std::string CodeSample;
    Language Lang;
  };

  using CodeFiles = StringMap<CodeEntry>;

  /// Builds an ASTUnit for one potential compile options set.
  SingleASTUnit createASTUnit(StringRef FileName, const CodeEntry &CE) const {
    ArgVector Args = getArgVectorForLanguage(CE.Lang);
    auto AST = tooling::buildASTFromCodeWithArgs(CE.CodeSample, Args, FileName);
    EXPECT_TRUE(AST.get());
    return AST;
  }

  /// Test an arbitrary sequence of imports for a set of given in-memory files.
  /// The verification is done by running VerificationMatcher against a
  /// specified AST node inside of one of given files.
  /// \param CodeSamples Map whose key is the file name and the value is the
  /// file content.
  /// \param ImportActions Sequence of imports. Each import in sequence
  /// specifies "from file" and "to file" and a matcher that is used for
  /// searching a declaration for import in "from file".
  /// \param FileForFinalCheck Name of virtual file for which the final check is
  /// applied.
  /// \param FinalSelectPredicate Matcher that specifies the AST node in the
  /// FileForFinalCheck for which the verification will be done.
  /// \param VerificationMatcher Matcher that will be used for verification
  /// after all imports in sequence are done.
  void testImportSequence(const CodeFiles &CodeSamples,
                          const std::vector<ImportAction> &ImportActions,
                          StringRef FileForFinalCheck,
                          BindableMatcher<Decl> FinalSelectPredicate,
                          BindableMatcher<Decl> VerificationMatcher) {
    AllASTUnits AllASTs;
    using ImporterKey = std::pair<const ASTUnit *, const ASTUnit *>;
    llvm::DenseMap<ImporterKey, std::unique_ptr<ASTImporter>> Importers;

    auto GenASTsIfNeeded = [this, &AllASTs, &CodeSamples](StringRef Filename) {
      if (!AllASTs.count(Filename)) {
        auto Found = CodeSamples.find(Filename);
        assert(Found != CodeSamples.end() && "Wrong file for import!");
        AllASTs[Filename] = createASTUnit(Filename, Found->getValue());
      }
    };

    for (const ImportAction &Action : ImportActions) {
      StringRef FromFile = Action.FromFilename, ToFile = Action.ToFilename;
      GenASTsIfNeeded(FromFile);
      GenASTsIfNeeded(ToFile);

      ASTUnit *From = AllASTs[FromFile].get();
      ASTUnit *To = AllASTs[ToFile].get();

      // Create a new importer if needed.
      std::unique_ptr<ASTImporter> &ImporterRef = Importers[{From, To}];
      if (!ImporterRef)
        ImporterRef.reset(new ASTImporter(
            To->getASTContext(), To->getFileManager(), From->getASTContext(),
            From->getFileManager(), false));

      // Find the declaration and import it.
      auto FoundDecl = match(Action.ImportPredicate.bind(DeclToImportID),
                             From->getASTContext());
      EXPECT_TRUE(FoundDecl.size() == 1);
      const Decl *ToImport = selectFirst<Decl>(DeclToImportID, FoundDecl);
      auto Imported = importNode(From, To, *ImporterRef, ToImport);
      EXPECT_TRUE(Imported);
    }

    // Find the declaration and import it.
    auto FoundDecl = match(FinalSelectPredicate.bind(DeclToVerifyID),
                           AllASTs[FileForFinalCheck]->getASTContext());
    EXPECT_TRUE(FoundDecl.size() == 1);
    const Decl *ToVerify = selectFirst<Decl>(DeclToVerifyID, FoundDecl);
    MatchVerifier<Decl> Verifier;
    EXPECT_TRUE(
        Verifier.match(ToVerify, BindableMatcher<Decl>(VerificationMatcher)));
  }
};

template <typename T> RecordDecl *getRecordDecl(T *D) {
  auto *ET = cast<ElaboratedType>(D->getType().getTypePtr());
  return cast<RecordType>(ET->getNamedType().getTypePtr())->getDecl();
}

// This class provides generic methods to write tests which can check internal
// attributes of AST nodes like getPreviousDecl(), isVirtual(), etc. Also,
// this fixture makes it possible to import from several "From" contexts.
class ASTImporterTestBase : public ParameterizedTestsFixture {

  const char *const InputFileName = "input.cc";
  const char *const OutputFileName = "output.cc";

  // Buffer for the To context, must live in the test scope.
  std::string ToCode;

  // Represents a "From" translation unit and holds an importer object which we
  // use to import from this translation unit.
  struct TU {
    // Buffer for the context, must live in the test scope.
    std::string Code;
    std::string FileName;
    std::unique_ptr<ASTUnit> Unit;
    TranslationUnitDecl *TUDecl = nullptr;
    std::unique_ptr<ASTImporter> Importer;
    TU(StringRef Code, StringRef FileName, ArgVector Args)
        : Code(Code), FileName(FileName),
          Unit(tooling::buildASTFromCodeWithArgs(this->Code, Args,
                                                 this->FileName)),
          TUDecl(Unit->getASTContext().getTranslationUnitDecl()) {
      Unit->enableSourceFileDiagnostics();
    }

    void lazyInitImporter(ASTUnit *ToAST) {
      assert(ToAST);
      if (!Importer) {
        Importer.reset(new ASTImporter(
            ToAST->getASTContext(), ToAST->getFileManager(),
            Unit->getASTContext(), Unit->getFileManager(), false));
      }
      assert(&ToAST->getASTContext() == &Importer->getToContext());
      createVirtualFileIfNeeded(ToAST, FileName, Code);
    }

    Decl *import(ASTUnit *ToAST, Decl *FromDecl) {
      lazyInitImporter(ToAST);
      return Importer->Import(FromDecl);
     }

    QualType import(ASTUnit *ToAST, QualType FromType) {
      lazyInitImporter(ToAST);
      return Importer->Import(FromType);
    }
  };

  // We may have several From contexts and related translation units. In each
  // AST, the buffers for the source are handled via references and are set
  // during the creation of the AST. These references must point to a valid
  // buffer until the AST is alive. Thus, we must use a list in order to avoid
  // moving of the stored objects because that would mean breaking the
  // references in the AST. By using a vector a move could happen when the
  // vector is expanding, with the list we won't have these issues.
  std::list<TU> FromTUs;

  void lazyInitToAST(Language ToLang) {
    if (ToAST)
      return;
    ArgVector ToArgs = getArgVectorForLanguage(ToLang);
    // Build the AST from an empty file.
    ToAST = tooling::buildASTFromCodeWithArgs(/*Code=*/"", ToArgs, "empty.cc");
    ToAST->enableSourceFileDiagnostics();
  }

  TU *findFromTU(Decl *From) {
    // Create a virtual file in the To Ctx which corresponds to the file from
    // which we want to import the `From` Decl. Without this source locations
    // will be invalid in the ToCtx.
    auto It = std::find_if(FromTUs.begin(), FromTUs.end(), [From](const TU &E) {
      return E.TUDecl == From->getTranslationUnitDecl();
    });
    assert(It != FromTUs.end());
    return &*It;
  }

public:
  // We may have several From context but only one To context.
  std::unique_ptr<ASTUnit> ToAST;

  // Creates an AST both for the From and To source code and imports the Decl
  // of the identifier into the To context.
  // Must not be called more than once within the same test.
  std::tuple<Decl *, Decl *>
  getImportedDecl(StringRef FromSrcCode, Language FromLang, StringRef ToSrcCode,
                  Language ToLang, StringRef Identifier = DeclToImportID) {
    ArgVector FromArgs = getArgVectorForLanguage(FromLang),
              ToArgs = getArgVectorForLanguage(ToLang);

    FromTUs.emplace_back(FromSrcCode, InputFileName, FromArgs);
    TU &FromTU = FromTUs.back();

    ToCode = ToSrcCode;
    assert(!ToAST);
    ToAST = tooling::buildASTFromCodeWithArgs(ToCode, ToArgs, OutputFileName);
    ToAST->enableSourceFileDiagnostics();

    ASTContext &FromCtx = FromTU.Unit->getASTContext();

    createVirtualFileIfNeeded(ToAST.get(), InputFileName, FromTU.Code);

    IdentifierInfo *ImportedII = &FromCtx.Idents.get(Identifier);
    assert(ImportedII && "Declaration with the given identifier "
                         "should be specified in test!");
    DeclarationName ImportDeclName(ImportedII);
    SmallVector<NamedDecl *, 4> FoundDecls;
    FromCtx.getTranslationUnitDecl()->localUncachedLookup(ImportDeclName,
                                                          FoundDecls);

    assert(FoundDecls.size() == 1);

    Decl *Imported = FromTU.import(ToAST.get(), FoundDecls.front());

    assert(Imported);
    return std::make_tuple(*FoundDecls.begin(), Imported);
  }

  // Creates a TU decl for the given source code which can be used as a From
  // context.  May be called several times in a given test (with different file
  // name).
  TranslationUnitDecl *getTuDecl(StringRef SrcCode, Language Lang,
                                 StringRef FileName = "input.cc") {
    assert(
        std::find_if(FromTUs.begin(), FromTUs.end(), [FileName](const TU &E) {
          return E.FileName == FileName;
        }) == FromTUs.end());

    ArgVector Args = getArgVectorForLanguage(Lang);
    FromTUs.emplace_back(SrcCode, FileName, Args);
    TU &Tu = FromTUs.back();

    return Tu.TUDecl;
  }

  // Creates the To context with the given source code and returns the TU decl.
  TranslationUnitDecl *getToTuDecl(StringRef ToSrcCode, Language ToLang) {
    ArgVector ToArgs = getArgVectorForLanguage(ToLang);
    ToCode = ToSrcCode;
    assert(!ToAST);
    ToAST = tooling::buildASTFromCodeWithArgs(ToCode, ToArgs, OutputFileName);
    ToAST->enableSourceFileDiagnostics();

    return ToAST->getASTContext().getTranslationUnitDecl();
  }

  // Import the given Decl into the ToCtx.
  // May be called several times in a given test.
  // The different instances of the param From may have different ASTContext.
  Decl *Import(Decl *From, Language ToLang) {
    lazyInitToAST(ToLang);
    TU *FromTU = findFromTU(From);
    return FromTU->import(ToAST.get(), From);
  }

  QualType ImportType(QualType FromType, Decl *TUDecl, Language ToLang) {
    lazyInitToAST(ToLang);
    TU *FromTU = findFromTU(TUDecl);
    return FromTU->import(ToAST.get(), FromType);
   }

  ~ASTImporterTestBase() {
    if (!::testing::Test::HasFailure()) return;

    for (auto &Tu : FromTUs) {
      assert(Tu.Unit);
      llvm::errs() << "FromAST:\n";
      Tu.Unit->getASTContext().getTranslationUnitDecl()->dump();
      llvm::errs() << "\n";
    }
    if (ToAST) {
      llvm::errs() << "ToAST:\n";
      ToAST->getASTContext().getTranslationUnitDecl()->dump();
    }
  }
};

struct ImportExpr : TestImportBase {};
struct ImportType : TestImportBase {};
struct ImportDecl : TestImportBase {};

struct CanonicalRedeclChain : ASTImporterTestBase {};

TEST_P(CanonicalRedeclChain, ShouldBeConsequentWithMatchers) {
  Decl *FromTU = getTuDecl("void f();", Lang_CXX);
  auto Pattern = functionDecl(hasName("f"));
  auto *D0 = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

  auto Redecls = getCanonicalForwardRedeclChain(D0);
  ASSERT_EQ(Redecls.size(), 1u);
  EXPECT_EQ(D0, Redecls[0]);
}

TEST_P(CanonicalRedeclChain, ShouldBeConsequentWithMatchers2) {
  Decl *FromTU = getTuDecl("void f(); void f(); void f();", Lang_CXX);
  auto Pattern = functionDecl(hasName("f"));
  auto *D0 = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
  auto *D2 = LastDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
  FunctionDecl *D1 = D2->getPreviousDecl();

  auto Redecls = getCanonicalForwardRedeclChain(D0);
  ASSERT_EQ(Redecls.size(), 3u);
  EXPECT_EQ(D0, Redecls[0]);
  EXPECT_EQ(D1, Redecls[1]);
  EXPECT_EQ(D2, Redecls[2]);
}

TEST_P(CanonicalRedeclChain, ShouldBeSameForAllDeclInTheChain) {
  Decl *FromTU = getTuDecl("void f(); void f(); void f();", Lang_CXX);
  auto Pattern = functionDecl(hasName("f"));
  auto *D0 = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
  auto *D2 = LastDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
  FunctionDecl *D1 = D2->getPreviousDecl();

  auto RedeclsD0 = getCanonicalForwardRedeclChain(D0);
  auto RedeclsD1 = getCanonicalForwardRedeclChain(D1);
  auto RedeclsD2 = getCanonicalForwardRedeclChain(D2);

  EXPECT_THAT(RedeclsD0, ::testing::ContainerEq(RedeclsD1));
  EXPECT_THAT(RedeclsD1, ::testing::ContainerEq(RedeclsD2));
}


TEST_P(ImportExpr, ImportStringLiteral) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() { (void)\"foo\"; }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(
          stringLiteral(hasType(asString("const char [4]"))))));
  testImport(
      "void declToImport() { (void)L\"foo\"; }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(
          stringLiteral(hasType(asString("const wchar_t [4]"))))));
  testImport(
      "void declToImport() { (void) \"foo\" \"bar\"; }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(
          stringLiteral(hasType(asString("const char [7]"))))));
}

TEST_P(ImportExpr, ImportGNUNullExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() { (void)__null; }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(gnuNullExpr(hasType(isInteger())))));
}

TEST_P(ImportExpr, ImportCXXNullPtrLiteralExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() { (void)nullptr; }",
      Lang_CXX11, "", Lang_CXX11, Verifier,
      functionDecl(hasDescendant(cxxNullPtrLiteralExpr())));
}


TEST_P(ImportExpr, ImportFloatinglLiteralExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() { (void)1.0; }",
      Lang_C, "", Lang_C, Verifier,
      functionDecl(hasDescendant(
          floatLiteral(equals(1.0), hasType(asString("double"))))));
  testImport(
      "void declToImport() { (void)1.0e-5f; }",
      Lang_C, "", Lang_C, Verifier,
      functionDecl(hasDescendant(
          floatLiteral(equals(1.0e-5f), hasType(asString("float"))))));
}

TEST_P(ImportExpr, ImportCompoundLiteralExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() {"
      "  struct s { int x; long y; unsigned z; }; "
      "  (void)(struct s){ 42, 0L, 1U }; }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(
          compoundLiteralExpr(
              hasType(asString("struct s")),
              has(initListExpr(
                  hasType(asString("struct s")),
                  has(integerLiteral(
                      equals(42), hasType(asString("int")))),
                  has(integerLiteral(
                      equals(0), hasType(asString("long")))),
                  has(integerLiteral(
                      equals(1), hasType(asString("unsigned int"))))))))));
}

TEST_P(ImportExpr, ImportCXXThisExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "class declToImport { void f() { (void)this; } };",
      Lang_CXX, "", Lang_CXX, Verifier,
      cxxRecordDecl(
          hasMethod(
              hasDescendant(
                  cxxThisExpr(
                      hasType(
                          asString("class declToImport *")))))));
}

TEST_P(ImportExpr, ImportAtomicExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() { int *ptr; __atomic_load_n(ptr, 1); }",
      Lang_C, "", Lang_C, Verifier,
      functionDecl(hasDescendant(
          atomicExpr(
              has(ignoringParenImpCasts(
                  declRefExpr(hasDeclaration(varDecl(hasName("ptr"))),
                      hasType(asString("int *"))))),
              has(integerLiteral(equals(1), hasType(asString("int"))))))));
}

TEST_P(ImportExpr, ImportLabelDeclAndAddrLabelExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() { loop: goto loop; (void)&&loop; }",
      Lang_C, "", Lang_C, Verifier,
      functionDecl(
          hasDescendant(
              labelStmt(hasDeclaration(labelDecl(hasName("loop"))))),
          hasDescendant(
              addrLabelExpr(hasDeclaration(labelDecl(hasName("loop")))))));
}

AST_MATCHER_P(TemplateDecl, hasTemplateDecl,
              internal::Matcher<NamedDecl>, InnerMatcher) {
  const NamedDecl *Template = Node.getTemplatedDecl();
  return Template && InnerMatcher.matches(*Template, Finder, Builder);
}

TEST_P(ImportExpr, ImportParenListExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template<typename T> class dummy { void f() { dummy X(*this); } };"
      "typedef dummy<int> declToImport;"
      "template class dummy<int>;",
      Lang_CXX, "", Lang_CXX, Verifier,
      typedefDecl(hasType(templateSpecializationType(
          hasDeclaration(classTemplateSpecializationDecl(hasSpecializedTemplate(
              classTemplateDecl(hasTemplateDecl(cxxRecordDecl(hasMethod(allOf(
                  hasName("f"),
                  hasBody(compoundStmt(has(declStmt(hasSingleDecl(
                      varDecl(hasInitializer(parenListExpr(has(unaryOperator(
                          hasOperatorName("*"),
                          hasUnaryOperand(cxxThisExpr())))))))))))))))))))))));
}

TEST_P(ImportExpr, ImportSwitch) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() { int b; switch (b) { case 1: break; } }",
      Lang_C, "", Lang_C, Verifier,
      functionDecl(hasDescendant(
          switchStmt(has(compoundStmt(has(caseStmt())))))));
}

TEST_P(ImportExpr, ImportStmtExpr) {
  MatchVerifier<Decl> Verifier;
  // NOTE: has() ignores implicit casts, using hasDescendant() to match it
  testImport(
    "void declToImport() { int b; int a = b ?: 1; int C = ({int X=4; X;}); }",
    Lang_C, "", Lang_C, Verifier,
    functionDecl(hasDescendant(
        varDecl(
            hasName("C"),
            hasType(asString("int")),
            hasInitializer(
                stmtExpr(
                    hasAnySubstatement(declStmt(hasSingleDecl(
                        varDecl(
                            hasName("X"),
                            hasType(asString("int")),
                            hasInitializer(
                                integerLiteral(equals(4))))))),
                    hasDescendant(
                        implicitCastExpr())))))));
}

TEST_P(ImportExpr, ImportConditionalOperator) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() { (void)(true ? 1 : -5); }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(
          conditionalOperator(
              hasCondition(cxxBoolLiteral(equals(true))),
              hasTrueExpression(integerLiteral(equals(1))),
              hasFalseExpression(
                  unaryOperator(hasUnaryOperand(integerLiteral(equals(5))))))
          )));
}

TEST_P(ImportExpr, ImportBinaryConditionalOperator) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() { (void)(1 ?: -5); }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(
          binaryConditionalOperator(
              hasCondition(
                  implicitCastExpr(
                      hasSourceExpression(opaqueValueExpr(
                          hasSourceExpression(integerLiteral(equals(1))))),
                      hasType(booleanType()))),
              hasTrueExpression(
                  opaqueValueExpr(
                      hasSourceExpression(integerLiteral(equals(1))))),
              hasFalseExpression(
                  unaryOperator(
                      hasOperatorName("-"),
                      hasUnaryOperand(integerLiteral(equals(5)))))))));
}

TEST_P(ImportExpr, ImportDesignatedInitExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() {"
      "  struct point { double x; double y; };"
      "  struct point ptarray[10] = "
      "{ [2].y = 1.0, [2].x = 2.0, [0].x = 1.0 }; }",
      Lang_C, "", Lang_C, Verifier,
      functionDecl(hasDescendant(
          initListExpr(
              has(designatedInitExpr(
                  designatorCountIs(2),
                  has(floatLiteral(equals(1.0))),
                  has(integerLiteral(equals(2))))),
              has(designatedInitExpr(
                  designatorCountIs(2),
                  has(floatLiteral(equals(2.0))),
                  has(integerLiteral(equals(2))))),
              has(designatedInitExpr(
                  designatorCountIs(2),
                  has(floatLiteral(equals(1.0))),
                  has(integerLiteral(equals(0)))))))));
}


TEST_P(ImportExpr, ImportPredefinedExpr) {
  MatchVerifier<Decl> Verifier;
  // __func__ expands as StringLiteral("declToImport")
  testImport(
      "void declToImport() { (void)__func__; }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(
          predefinedExpr(
              hasType(
                  asString("const char [13]")),
              has(stringLiteral(hasType(
                  asString("const char [13]"))))))));
}

TEST_P(ImportExpr, ImportInitListExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() {"
      "  struct point { double x; double y; };"
      "  point ptarray[10] = { [2].y = 1.0, [2].x = 2.0,"
      "                        [0].x = 1.0 }; }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(
          initListExpr(
              has(
                  cxxConstructExpr(
                  requiresZeroInitialization())),
              has(
                  initListExpr(
                      hasType(asString("struct point")),
                      has(floatLiteral(equals(1.0))),
                      has(implicitValueInitExpr(
                          hasType(asString("double")))))),
              has(
                  initListExpr(
                      hasType(asString("struct point")),
                      has(floatLiteral(equals(2.0))),
                      has(floatLiteral(equals(1.0)))))))));
}


const internal::VariadicDynCastAllOfMatcher<Expr, VAArgExpr> vaArgExpr;

TEST_P(ImportExpr, ImportVAArgExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport(__builtin_va_list list, ...) {"
      "  (void)__builtin_va_arg(list, int); }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(
          cStyleCastExpr(hasSourceExpression(vaArgExpr())))));
}

TEST_P(ImportExpr, CXXTemporaryObjectExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "struct C {};"
      "void declToImport() { C c = C(); }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(
          exprWithCleanups(has(cxxConstructExpr(
              has(materializeTemporaryExpr(has(implicitCastExpr(
                  has(cxxTemporaryObjectExpr())))))))))));
}

TEST_P(ImportType, ImportAtomicType) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() { typedef _Atomic(int) a_int; }",
      Lang_CXX11, "", Lang_CXX11, Verifier,
      functionDecl(hasDescendant(typedefDecl(has(atomicType())))));
}

TEST_P(ImportDecl, ImportFunctionTemplateDecl) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template <typename T> void declToImport() { };",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionTemplateDecl());
}

const internal::VariadicDynCastAllOfMatcher<Expr, CXXDependentScopeMemberExpr>
    cxxDependentScopeMemberExpr;

TEST_P(ImportExpr, ImportCXXDependentScopeMemberExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template <typename T> struct C { T t; };"
      "template <typename T> void declToImport() {"
      "  C<T> d;"
      "  (void)d.t;"
      "}"
      "void instantiate() { declToImport<int>(); }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionTemplateDecl(hasDescendant(
          cStyleCastExpr(has(cxxDependentScopeMemberExpr())))));
  testImport(
      "template <typename T> struct C { T t; };"
      "template <typename T> void declToImport() {"
      "  C<T> d;"
      "  (void)(&d)->t;"
      "}"
      "void instantiate() { declToImport<int>(); }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionTemplateDecl(hasDescendant(
          cStyleCastExpr(has(cxxDependentScopeMemberExpr())))));
}

TEST_P(ImportType, ImportTypeAliasTemplate) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template <int K>"
      "struct dummy { static const int i = K; };"
      "template <int K> using dummy2 = dummy<K>;"
      "int declToImport() { return dummy2<3>::i; }",
      Lang_CXX11, "", Lang_CXX11, Verifier,
      functionDecl(
          hasDescendant(implicitCastExpr(has(declRefExpr()))),
          unless(hasAncestor(translationUnitDecl(has(typeAliasDecl()))))));
}

const internal::VariadicDynCastAllOfMatcher<Decl, VarTemplateSpecializationDecl>
    varTemplateSpecializationDecl;

TEST_P(ImportDecl, ImportVarTemplate) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template <typename T>"
      "T pi = T(3.1415926535897932385L);"
      "void declToImport() { (void)pi<int>; }",
      Lang_CXX14, "", Lang_CXX14, Verifier,
      functionDecl(
          hasDescendant(declRefExpr(to(varTemplateSpecializationDecl()))),
          unless(hasAncestor(translationUnitDecl(has(varDecl(
              hasName("pi"), unless(varTemplateSpecializationDecl()))))))));
}

TEST_P(ImportType, ImportPackExpansion) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template <typename... Args>"
      "struct dummy {"
      "  dummy(Args... args) {}"
      "  static const int i = 4;"
      "};"
      "int declToImport() { return dummy<int>::i; }",
      Lang_CXX11, "", Lang_CXX11, Verifier,
      functionDecl(hasDescendant(
          returnStmt(has(implicitCastExpr(has(declRefExpr())))))));
}

const internal::VariadicDynCastAllOfMatcher<Type,
                                            DependentTemplateSpecializationType>
    dependentTemplateSpecializationType;

TEST_P(ImportType, ImportDependentTemplateSpecialization) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template<typename T>"
      "struct A;"
      "template<typename T>"
      "struct declToImport {"
      "  typename A<T>::template B<T> a;"
      "};",
      Lang_CXX, "", Lang_CXX, Verifier,
      classTemplateDecl(has(cxxRecordDecl(has(
          fieldDecl(hasType(dependentTemplateSpecializationType())))))));
}

const internal::VariadicDynCastAllOfMatcher<Stmt, SizeOfPackExpr>
    sizeOfPackExpr;

TEST_P(ImportExpr, ImportSizeOfPackExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template <typename... Ts>"
      "void declToImport() {"
      "  const int i = sizeof...(Ts);"
      "};"
      "void g() { declToImport<int>(); }",
      Lang_CXX11, "", Lang_CXX11, Verifier,
          functionTemplateDecl(hasDescendant(sizeOfPackExpr())));
  testImport(
      "template <typename... Ts>"
      "using X = int[sizeof...(Ts)];"
      "template <typename... Us>"
      "struct Y {"
      "  X<Us..., int, double, int, Us...> f;"
      "};"
      "Y<float, int> declToImport;",
      Lang_CXX11, "", Lang_CXX11, Verifier,
      varDecl(hasType(classTemplateSpecializationDecl(has(fieldDecl(hasType(
          hasUnqualifiedDesugaredType(constantArrayType(hasSize(7))))))))));
}

/// \brief Matches __builtin_types_compatible_p:
/// GNU extension to check equivalent types
/// Given
/// \code
///   __builtin_types_compatible_p(int, int)
/// \endcode
//  will generate TypeTraitExpr <...> 'int'
const internal::VariadicDynCastAllOfMatcher<Stmt, TypeTraitExpr> typeTraitExpr;

TEST_P(ImportExpr, ImportTypeTraitExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "void declToImport() { "
      "  (void)__builtin_types_compatible_p(int, int);"
      "}",
      Lang_C, "", Lang_C, Verifier,
      functionDecl(hasDescendant(typeTraitExpr(hasType(asString("int"))))));
}

const internal::VariadicDynCastAllOfMatcher<Stmt, CXXTypeidExpr> cxxTypeidExpr;

TEST_P(ImportExpr, ImportCXXTypeidExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "namespace std { class type_info {}; }"
      "void declToImport() {"
      "  int x;"
      "  auto a = typeid(int); auto b = typeid(x);"
      "}",
      Lang_CXX11, "", Lang_CXX11, Verifier,
      functionDecl(
          hasDescendant(varDecl(
              hasName("a"), hasInitializer(hasDescendant(cxxTypeidExpr())))),
          hasDescendant(varDecl(
              hasName("b"), hasInitializer(hasDescendant(cxxTypeidExpr()))))));
}

TEST_P(ImportExpr, ImportTypeTraitExprValDep) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template<typename T> struct declToImport {"
      "  void m() { (void)__is_pod(T); }"
      "};"
      "void f() { declToImport<int>().m(); }",
      Lang_CXX11, "", Lang_CXX11, Verifier,
      classTemplateDecl(has(cxxRecordDecl(has(
          functionDecl(hasDescendant(
              typeTraitExpr(hasType(booleanType())))))))));
}

TEST_P(ImportDecl, ImportRecordDeclInFunc) {
  MatchVerifier<Decl> Verifier;
  testImport("int declToImport() { "
             "  struct data_t {int a;int b;};"
             "  struct data_t d;"
             "  return 0;"
             "}",
             Lang_C, "", Lang_C, Verifier,
             functionDecl(hasBody(compoundStmt(
                 has(declStmt(hasSingleDecl(varDecl(hasName("d")))))))));
}

TEST_P(ASTImporterTestBase, ImportRecordTypeInFunc) {
  Decl *FromTU = getTuDecl("int declToImport() { "
                           "  struct data_t {int a;int b;};"
                           "  struct data_t d;"
                           "  return 0;"
                           "}",
                           Lang_C, "input.c");
  auto FromVar =
      FirstDeclMatcher<VarDecl>().match(FromTU, varDecl(hasName("d")));
  ASSERT_TRUE(FromVar);
  auto ToType =
      ImportType(FromVar->getType().getCanonicalType(), FromVar, Lang_C);
  EXPECT_FALSE(ToType.isNull());
}

TEST_P(ASTImporterTestBase, ImportRecordDeclInFuncParams) {
  // This construct is not supported by ASTImporter.
  Decl *FromTU =
      getTuDecl("int declToImport(struct data_t{int a;int b;} *d){ return 0; }",
                Lang_C, "input.c");
  auto From = FirstDeclMatcher<FunctionDecl>().match(FromTU, functionDecl());
  ASSERT_TRUE(From);
  auto To = Import(From, Lang_C);
  EXPECT_EQ(To, nullptr);
}

const internal::VariadicDynCastAllOfMatcher<Expr, CXXPseudoDestructorExpr>
    cxxPseudoDestructorExpr;

TEST_P(ImportExpr, ImportCXXPseudoDestructorExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "typedef int T;"
      "void declToImport(int *p) {"
      "  T t;"
      "  p->T::~T();"
      "}",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(
          callExpr(has(cxxPseudoDestructorExpr())))));
}

TEST_P(ImportDecl, ImportUsingDecl) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "namespace foo { int bar; }"
      "void declToImport() { using foo::bar; }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionDecl(hasDescendant(usingDecl())));
}

/// \brief Matches shadow declarations introduced into a scope by a
///        (resolved) using declaration.
///
/// Given
/// \code
///   namespace n { int f; }
///   namespace declToImport { using n::f; }
/// \endcode
/// usingShadowDecl()
///   matches \code f \endcode
const internal::VariadicDynCastAllOfMatcher<Decl,
                                            UsingShadowDecl> usingShadowDecl;

TEST_P(ImportDecl, ImportUsingShadowDecl) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "namespace foo { int bar; }"
      "namespace declToImport { using foo::bar; }",
      Lang_CXX, "", Lang_CXX, Verifier,
      namespaceDecl(has(usingShadowDecl())));
}

TEST_P(ImportExpr, ImportUnresolvedLookupExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template<typename T> int foo();"
      "template <typename T> void declToImport() {"
      "  (void)::foo<T>;"
      "  (void)::template foo<T>;"
      "}"
      "void instantiate() { declToImport<int>(); }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionTemplateDecl(hasDescendant(unresolvedLookupExpr())));
}

TEST_P(ImportExpr, ImportCXXUnresolvedConstructExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template <typename T> struct C { T t; };"
      "template <typename T> void declToImport() {"
      "  C<T> d;"
      "  d.t = T();"
      "}"
      "void instantiate() { declToImport<int>(); }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionTemplateDecl(hasDescendant(
          binaryOperator(has(cxxUnresolvedConstructExpr())))));
  testImport(
      "template <typename T> struct C { T t; };"
      "template <typename T> void declToImport() {"
      "  C<T> d;"
      "  (&d)->t = T();"
      "}"
      "void instantiate() { declToImport<int>(); }",
      Lang_CXX, "", Lang_CXX, Verifier,
          functionTemplateDecl(hasDescendant(
              binaryOperator(has(cxxUnresolvedConstructExpr())))));
}

/// Check that function "declToImport()" (which is the templated function
/// for corresponding FunctionTemplateDecl) is not added into DeclContext.
/// Same for class template declarations.
TEST_P(ImportDecl, ImportTemplatedDeclForTemplate) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template <typename T> void declToImport() { T a = 1; }"
      "void instantiate() { declToImport<int>(); }",
      Lang_CXX, "", Lang_CXX, Verifier,
      functionTemplateDecl(hasAncestor(translationUnitDecl(
          unless(has(functionDecl(hasName("declToImport"))))))));
  testImport(
      "template <typename T> struct declToImport { T t; };"
      "void instantiate() { declToImport<int>(); }",
      Lang_CXX, "", Lang_CXX, Verifier,
      classTemplateDecl(hasAncestor(translationUnitDecl(
          unless(has(cxxRecordDecl(hasName("declToImport"))))))));
}

TEST_P(ImportDecl, ImportClassTemplatePartialSpecialization) {
  MatchVerifier<Decl> Verifier;
  auto Code =
      R"s(
      struct declToImport {
        template <typename T0> struct X;
        template <typename T0> struct X<T0 *> {};
      };
      )s";
  testImport(Code, Lang_CXX, "", Lang_CXX, Verifier,
             recordDecl(has(classTemplateDecl()),
                        has(classTemplateSpecializationDecl())));
}

TEST_P(ImportExpr, CXXOperatorCallExpr) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "class declToImport {"
      "  void f() { *this = declToImport(); }"
      "};",
      Lang_CXX, "", Lang_CXX, Verifier,
      cxxRecordDecl(has(cxxMethodDecl(hasDescendant(
          cxxOperatorCallExpr())))));
}

TEST_P(ImportExpr, DependentSizedArrayType) {
  MatchVerifier<Decl> Verifier;
  testImport(
      "template<typename T, int Size> class declToImport {"
      "  T data[Size];"
      "};",
      Lang_CXX, "", Lang_CXX, Verifier,
      classTemplateDecl(has(cxxRecordDecl(
          has(fieldDecl(hasType(dependentSizedArrayType())))))));
}

TEST_P(ASTImporterTestBase, ImportOfTemplatedDeclOfClassTemplateDecl) {
  Decl *FromTU = getTuDecl("template<class X> struct S{};", Lang_CXX);
  auto From =
      FirstDeclMatcher<ClassTemplateDecl>().match(FromTU, classTemplateDecl());
  ASSERT_TRUE(From);
  auto To = cast<ClassTemplateDecl>(Import(From, Lang_CXX));
  ASSERT_TRUE(To);
  Decl *ToTemplated = To->getTemplatedDecl();
  Decl *ToTemplated1 = Import(From->getTemplatedDecl(), Lang_CXX);
  EXPECT_TRUE(ToTemplated1);
  EXPECT_EQ(ToTemplated1, ToTemplated);
}

TEST_P(ASTImporterTestBase, ImportOfTemplatedDeclOfFunctionTemplateDecl) {
  Decl *FromTU = getTuDecl("template<class X> void f(){}", Lang_CXX);
  auto From = FirstDeclMatcher<FunctionTemplateDecl>().match(
      FromTU, functionTemplateDecl());
  ASSERT_TRUE(From);
  auto To = cast<FunctionTemplateDecl>(Import(From, Lang_CXX));
  ASSERT_TRUE(To);
  Decl *ToTemplated = To->getTemplatedDecl();
  Decl *ToTemplated1 = Import(From->getTemplatedDecl(), Lang_CXX);
  EXPECT_TRUE(ToTemplated1);
  EXPECT_EQ(ToTemplated1, ToTemplated);
}

TEST_P(ASTImporterTestBase,
       ImportOfTemplatedDeclShouldImportTheClassTemplateDecl) {
  Decl *FromTU = getTuDecl("template<class X> struct S{};", Lang_CXX);
  auto FromFT =
      FirstDeclMatcher<ClassTemplateDecl>().match(FromTU, classTemplateDecl());
  ASSERT_TRUE(FromFT);

  auto ToTemplated =
      cast<CXXRecordDecl>(Import(FromFT->getTemplatedDecl(), Lang_CXX));
  EXPECT_TRUE(ToTemplated);
  auto ToTU = ToTemplated->getTranslationUnitDecl();
  auto ToFT =
      FirstDeclMatcher<ClassTemplateDecl>().match(ToTU, classTemplateDecl());
  EXPECT_TRUE(ToFT);
}

TEST_P(ASTImporterTestBase,
       ImportOfTemplatedDeclShouldImportTheFunctionTemplateDecl) {
  Decl *FromTU = getTuDecl("template<class X> void f(){}", Lang_CXX);
  auto FromFT = FirstDeclMatcher<FunctionTemplateDecl>().match(
      FromTU, functionTemplateDecl());
  ASSERT_TRUE(FromFT);

  auto ToTemplated =
      cast<FunctionDecl>(Import(FromFT->getTemplatedDecl(), Lang_CXX));
  EXPECT_TRUE(ToTemplated);
  auto ToTU = ToTemplated->getTranslationUnitDecl();
  auto ToFT = FirstDeclMatcher<FunctionTemplateDecl>().match(
      ToTU, functionTemplateDecl());
  EXPECT_TRUE(ToFT);
}

TEST_P(ASTImporterTestBase, ImportCorrectTemplatedDecl) {
  auto Code =
        R"(
        namespace x {
          template<class X> struct S1{};
          template<class X> struct S2{};
          template<class X> struct S3{};
        }
        )";
  Decl *FromTU = getTuDecl(Code, Lang_CXX);
  auto FromNs =
      FirstDeclMatcher<NamespaceDecl>().match(FromTU, namespaceDecl());
  auto ToNs = cast<NamespaceDecl>(Import(FromNs, Lang_CXX));
  ASSERT_TRUE(ToNs);
  auto From =
      FirstDeclMatcher<ClassTemplateDecl>().match(FromTU,
                                                  classTemplateDecl(
                                                      hasName("S2")));
  auto To =
      FirstDeclMatcher<ClassTemplateDecl>().match(ToNs,
                                                  classTemplateDecl(
                                                      hasName("S2")));
  ASSERT_TRUE(From);
  ASSERT_TRUE(To);
  auto ToTemplated = To->getTemplatedDecl();
  auto ToTemplated1 =
      cast<CXXRecordDecl>(Import(From->getTemplatedDecl(), Lang_CXX));
  EXPECT_TRUE(ToTemplated1);
  ASSERT_EQ(ToTemplated1, ToTemplated);
}

TEST_P(ASTImporterTestBase, ImportFunctionWithBackReferringParameter) {
  Decl *From, *To;
  std::tie(From, To) = getImportedDecl(
      R"(
      template <typename T> struct X {};

      void declToImport(int y, X<int> &x) {}

      template <> struct X<int> {
        void g() {
          X<int> x;
          declToImport(0, x);
        }
      };
      )",
      Lang_CXX, "", Lang_CXX);

  MatchVerifier<Decl> Verifier;
  auto Matcher = functionDecl(hasName("declToImport"),
                              parameterCountIs(2),
                              hasParameter(0, hasName("y")),
                              hasParameter(1, hasName("x")),
                              hasParameter(1, hasType(asString("X<int> &"))));
  ASSERT_TRUE(Verifier.match(From, Matcher));
  EXPECT_TRUE(Verifier.match(To, Matcher));
}

TEST_P(ASTImporterTestBase,
       TUshouldNotContainTemplatedDeclOfFunctionTemplates) {
  Decl *From, *To;
  std::tie(From, To) =
      getImportedDecl("template <typename T> void declToImport() { T a = 1; }"
                      "void instantiate() { declToImport<int>(); }",
                      Lang_CXX, "", Lang_CXX);

  auto Check = [](Decl *D) -> bool {
    auto TU = D->getTranslationUnitDecl();
    for (auto Child : TU->decls()) {
      if (auto *FD = dyn_cast<FunctionDecl>(Child)) {
        if (FD->getNameAsString() == "declToImport") {
          GTEST_NONFATAL_FAILURE_(
              "TU should not contain any FunctionDecl with name declToImport");
          return false;
        }
      }
    }
    return true;
  };

  ASSERT_TRUE(Check(From));
  EXPECT_TRUE(Check(To));
}

TEST_P(ASTImporterTestBase, TUshouldNotContainTemplatedDeclOfClassTemplates) {
  Decl *From, *To;
  std::tie(From, To) =
      getImportedDecl("template <typename T> struct declToImport { T t; };"
                      "void instantiate() { declToImport<int>(); }",
                      Lang_CXX, "", Lang_CXX);

  auto Check = [](Decl *D) -> bool {
    auto TU = D->getTranslationUnitDecl();
    for (auto Child : TU->decls()) {
      if (auto *RD = dyn_cast<CXXRecordDecl>(Child)) {
        if (RD->getNameAsString() == "declToImport") {
          GTEST_NONFATAL_FAILURE_(
              "TU should not contain any CXXRecordDecl with name declToImport");
          return false;
        }
      }
    }
    return true;
  };

  ASSERT_TRUE(Check(From));
  EXPECT_TRUE(Check(To));
}

TEST_P(ASTImporterTestBase, TUshouldNotContainTemplatedDeclOfTypeAlias) {
  Decl *From, *To;
  std::tie(From, To) =
      getImportedDecl(
          "template <typename T> struct X {};"
          "template <typename T> using declToImport = X<T>;"
          "void instantiate() { declToImport<int> a; }",
                      Lang_CXX11, "", Lang_CXX11);

  auto Check = [](Decl *D) -> bool {
    auto TU = D->getTranslationUnitDecl();
    for (auto Child : TU->decls()) {
      if (auto *AD = dyn_cast<TypeAliasDecl>(Child)) {
        if (AD->getNameAsString() == "declToImport") {
          GTEST_NONFATAL_FAILURE_(
              "TU should not contain any TypeAliasDecl with name declToImport");
          return false;
        }
      }
    }
    return true;
  };

  ASSERT_TRUE(Check(From));
  EXPECT_TRUE(Check(To));
}

TEST_P(
    ASTImporterTestBase,
    TUshouldNotContainClassTemplateSpecializationOfImplicitInstantiation) {

  Decl *From, *To;
  std::tie(From, To) = getImportedDecl(
      R"(
      template<class T>
      class Base {};
      class declToImport : public Base<declToImport> {};
      )",
      Lang_CXX, "", Lang_CXX);

  // Check that the ClassTemplateSpecializationDecl is NOT the child of the TU.
  auto Pattern =
      translationUnitDecl(unless(has(classTemplateSpecializationDecl())));
  ASSERT_TRUE(
      MatchVerifier<Decl>{}.match(From->getTranslationUnitDecl(), Pattern));
  EXPECT_TRUE(
      MatchVerifier<Decl>{}.match(To->getTranslationUnitDecl(), Pattern));

  // Check that the ClassTemplateSpecializationDecl is the child of the
  // ClassTemplateDecl.
  Pattern = translationUnitDecl(has(classTemplateDecl(
      hasName("Base"), has(classTemplateSpecializationDecl()))));
  ASSERT_TRUE(
      MatchVerifier<Decl>{}.match(From->getTranslationUnitDecl(), Pattern));
  EXPECT_TRUE(
      MatchVerifier<Decl>{}.match(To->getTranslationUnitDecl(), Pattern));
}

AST_MATCHER_P(RecordDecl, hasFieldOrder, std::vector<StringRef>, Order) {
  size_t Index = 0;
  for (FieldDecl *Field : Node.fields()) {
    if (Index == Order.size())
      return false;
    if (Field->getName() != Order[Index])
      return false;
    ++Index;
  }
  return Index == Order.size();
}

TEST_P(ASTImporterTestBase,
       TUshouldContainClassTemplateSpecializationOfExplicitInstantiation) {
  Decl *From, *To;
  std::tie(From, To) = getImportedDecl(
      R"(
      namespace NS {
        template<class T>
        class X {};
        template class X<int>;
      }
      )",
      Lang_CXX, "", Lang_CXX, "NS");

  // Check that the ClassTemplateSpecializationDecl is NOT the child of the
  // ClassTemplateDecl.
  auto Pattern = namespaceDecl(has(classTemplateDecl(
      hasName("X"), unless(has(classTemplateSpecializationDecl())))));
  ASSERT_TRUE(MatchVerifier<Decl>{}.match(From, Pattern));
  EXPECT_TRUE(MatchVerifier<Decl>{}.match(To, Pattern));

  // Check that the ClassTemplateSpecializationDecl is the child of the
  // NamespaceDecl.
  Pattern = namespaceDecl(has(classTemplateSpecializationDecl(hasName("X"))));
  ASSERT_TRUE(MatchVerifier<Decl>{}.match(From, Pattern));
  EXPECT_TRUE(MatchVerifier<Decl>{}.match(To, Pattern));
}

TEST_P(ASTImporterTestBase, CXXRecordDeclFieldsShouldBeInCorrectOrder) {
  Decl *From, *To;
  std::tie(From, To) =
      getImportedDecl(
          "struct declToImport { int a; int b; };",
                      Lang_CXX11, "", Lang_CXX11);

  MatchVerifier<Decl> Verifier;
  ASSERT_TRUE(Verifier.match(From, cxxRecordDecl(hasFieldOrder({"a", "b"}))));
  EXPECT_TRUE(Verifier.match(To, cxxRecordDecl(hasFieldOrder({"a", "b"}))));
}

TEST_P(ASTImporterTestBase,
       DISABLED_CXXRecordDeclFieldOrderShouldNotDependOnImportOrder) {
  Decl *From, *To;
  std::tie(From, To) = getImportedDecl(
      // The original recursive algorithm of ASTImporter first imports 'c' then
      // 'b' and lastly 'a'.  Therefore we must restore the order somehow.
      R"s(
      struct declToImport {
          int a = c + b;
          int b = 1;
          int c = 2;
      };
      )s",
      Lang_CXX11, "", Lang_CXX11);

  MatchVerifier<Decl> Verifier;
  ASSERT_TRUE(
      Verifier.match(From, cxxRecordDecl(hasFieldOrder({"a", "b", "c"}))));
  EXPECT_TRUE(
      Verifier.match(To, cxxRecordDecl(hasFieldOrder({"a", "b", "c"}))));
}

TEST_P(ASTImporterTestBase, ShouldImportImplicitCXXRecordDecl) {
  Decl *From, *To;
  std::tie(From, To) = getImportedDecl(
      R"(
      struct declToImport {
      };
      )",
      Lang_CXX, "", Lang_CXX);

  MatchVerifier<Decl> Verifier;
  // Match the implicit Decl.
  auto Matcher = cxxRecordDecl(has(cxxRecordDecl()));
  ASSERT_TRUE(Verifier.match(From, Matcher));
  EXPECT_TRUE(Verifier.match(To, Matcher));
}

TEST_P(ASTImporterTestBase, ShouldImportImplicitCXXRecordDeclOfClassTemplate) {
  Decl *From, *To;
  std::tie(From, To) = getImportedDecl(
      R"(
      template <typename U>
      struct declToImport {
      };
      )",
      Lang_CXX, "", Lang_CXX);

  MatchVerifier<Decl> Verifier;
  // Match the implicit Decl.
  auto Matcher = classTemplateDecl(has(cxxRecordDecl(has(cxxRecordDecl()))));
  ASSERT_TRUE(Verifier.match(From, Matcher));
  EXPECT_TRUE(Verifier.match(To, Matcher));
}

TEST_P(
    ASTImporterTestBase,
    ShouldImportImplicitCXXRecordDeclOfClassTemplateSpecializationDecl) {
  Decl *From, *To;
  std::tie(From, To) = getImportedDecl(
      R"(
      template<class T>
      class Base {};
      class declToImport : public Base<declToImport> {};
      )",
      Lang_CXX, "", Lang_CXX);

  auto hasImplicitClass = has(cxxRecordDecl());
  auto Pattern = translationUnitDecl(has(classTemplateDecl(
      hasName("Base"),
      has(classTemplateSpecializationDecl(hasImplicitClass)))));
  ASSERT_TRUE(
      MatchVerifier<Decl>{}.match(From->getTranslationUnitDecl(), Pattern));
  EXPECT_TRUE(
      MatchVerifier<Decl>{}.match(To->getTranslationUnitDecl(), Pattern));
}

TEST_P(ASTImporterTestBase, IDNSOrdinary) {
  Decl *From, *To;
  std::tie(From, To) =
      getImportedDecl("void declToImport() {}", Lang_CXX, "", Lang_CXX);

  MatchVerifier<Decl> Verifier;
  auto Matcher = functionDecl();
  ASSERT_TRUE(Verifier.match(From, Matcher));
  EXPECT_TRUE(Verifier.match(To, Matcher));
  EXPECT_EQ(From->getIdentifierNamespace(), To->getIdentifierNamespace());
}

TEST_P(ASTImporterTestBase, IDNSOfNonmemberOperator) {
  Decl *FromTU = getTuDecl(
      R"(
      struct X {};
      void operator<<(int, X);
      )",
      Lang_CXX);
  Decl *From = LastDeclMatcher<Decl>{}.match(FromTU, functionDecl());
  const Decl *To = Import(From, Lang_CXX);
  EXPECT_EQ(From->getIdentifierNamespace(), To->getIdentifierNamespace());
}

TEST_P(ASTImporterTestBase,
       ShouldImportMembersOfClassTemplateSpecializationDecl) {
  Decl *From, *To;
  std::tie(From, To) = getImportedDecl(
      R"(
      template<class T>
      class Base { int a; };
      class declToImport : Base<declToImport> {};
      )",
      Lang_CXX, "", Lang_CXX);

  auto Pattern = translationUnitDecl(has(classTemplateDecl(
      hasName("Base"),
      has(classTemplateSpecializationDecl(has(fieldDecl(hasName("a"))))))));
  ASSERT_TRUE(
      MatchVerifier<Decl>{}.match(From->getTranslationUnitDecl(), Pattern));
  EXPECT_TRUE(
      MatchVerifier<Decl>{}.match(To->getTranslationUnitDecl(), Pattern));
}

TEST_P(ASTImporterTestBase, ImportDefinitionOfClassTemplateAfterFwdDecl) {
  {
    Decl *FromTU = getTuDecl(
        R"(
            template <typename T>
            struct B;
            )",
        Lang_CXX, "input0.cc");
    auto *FromD = FirstDeclMatcher<ClassTemplateDecl>().match(
        FromTU, classTemplateDecl(hasName("B")));

    Import(FromD, Lang_CXX);
  }

  {
    Decl *FromTU = getTuDecl(
        R"(
            template <typename T>
            struct B {
              void f();
            };
            )",
        Lang_CXX, "input1.cc");
    FunctionDecl *FromD = FirstDeclMatcher<FunctionDecl>().match(
        FromTU, functionDecl(hasName("f")));
    Import(FromD, Lang_CXX);
    auto *FromCTD = FirstDeclMatcher<ClassTemplateDecl>().match(
        FromTU, classTemplateDecl(hasName("B")));
    auto *ToCTD = cast<ClassTemplateDecl>(Import(FromCTD, Lang_CXX));
    EXPECT_TRUE(ToCTD->isThisDeclarationADefinition());
  }
}

TEST_P(ASTImporterTestBase,
       ImportDefinitionOfClassTemplateIfThereIsAnExistingFwdDeclAndDefinition) {
  Decl *ToTU = getToTuDecl(
      R"(
      template <typename T>
      struct B {
        void f();
      };

      template <typename T>
      struct B;
      )",
      Lang_CXX);
  ASSERT_EQ(1u, DeclCounterWithPredicate<ClassTemplateDecl>(
                    [](const ClassTemplateDecl *T) {
                      return T->isThisDeclarationADefinition();
                    })
                    .match(ToTU, classTemplateDecl()));

  Decl *FromTU = getTuDecl(
      R"(
      template <typename T>
      struct B {
        void f();
      };
      )",
      Lang_CXX, "input1.cc");
  ClassTemplateDecl *FromD = FirstDeclMatcher<ClassTemplateDecl>().match(
      FromTU, classTemplateDecl(hasName("B")));

  Import(FromD, Lang_CXX);

  // We should have only one definition.
  EXPECT_EQ(1u, DeclCounterWithPredicate<ClassTemplateDecl>(
                    [](const ClassTemplateDecl *T) {
                      return T->isThisDeclarationADefinition();
                    })
                    .match(ToTU, classTemplateDecl()));
}

TEST_P(ASTImporterTestBase,
       ImportDefinitionOfClassIfThereIsAnExistingFwdDeclAndDefinition) {
  Decl *ToTU = getToTuDecl(
      R"(
      struct B {
        void f();
      };

      struct B;
      )",
      Lang_CXX);
  ASSERT_EQ(2u, DeclCounter<CXXRecordDecl>().match(
                    ToTU, cxxRecordDecl(unless(isImplicit()))));

  Decl *FromTU = getTuDecl(
      R"(
      struct B {
        void f();
      };
      )",
      Lang_CXX, "input1.cc");
  auto *FromD = FirstDeclMatcher<CXXRecordDecl>().match(
      FromTU, cxxRecordDecl(hasName("B")));

  Import(FromD, Lang_CXX);

  EXPECT_EQ(2u, DeclCounter<CXXRecordDecl>().match(
                    ToTU, cxxRecordDecl(unless(isImplicit()))));
}

static void CompareSourceLocs(FullSourceLoc Loc1, FullSourceLoc Loc2) {
  EXPECT_EQ(Loc1.getExpansionLineNumber(), Loc2.getExpansionLineNumber());
  EXPECT_EQ(Loc1.getExpansionColumnNumber(), Loc2.getExpansionColumnNumber());
  EXPECT_EQ(Loc1.getSpellingLineNumber(), Loc2.getSpellingLineNumber());
  EXPECT_EQ(Loc1.getSpellingColumnNumber(), Loc2.getSpellingColumnNumber());
}
static void CompareSourceRanges(SourceRange Range1, SourceRange Range2,
                                SourceManager &SM1, SourceManager &SM2) {
  CompareSourceLocs(FullSourceLoc{ Range1.getBegin(), SM1 },
                    FullSourceLoc{ Range2.getBegin(), SM2 });
  CompareSourceLocs(FullSourceLoc{ Range1.getEnd(), SM1 },
                    FullSourceLoc{ Range2.getEnd(), SM2 });
}
TEST_P(ASTImporterTestBase, ImportSourceLocs) {
  Decl *FromTU = getTuDecl(
      R"(
      #define MFOO(arg) arg = arg + 1

      void foo() {
        int a = 5;
        MFOO(a);
      }
      )",
      Lang_CXX);
  auto FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, functionDecl());
  auto ToD = Import(FromD, Lang_CXX);

  auto ToLHS = LastDeclMatcher<DeclRefExpr>().match(ToD, declRefExpr());
  auto FromLHS = LastDeclMatcher<DeclRefExpr>().match(FromTU, declRefExpr());
  auto ToRHS = LastDeclMatcher<IntegerLiteral>().match(ToD, integerLiteral());
  auto FromRHS =
      LastDeclMatcher<IntegerLiteral>().match(FromTU, integerLiteral());

  SourceManager &ToSM = ToAST->getASTContext().getSourceManager();
  SourceManager &FromSM = FromD->getASTContext().getSourceManager();
  CompareSourceRanges(ToD->getSourceRange(), FromD->getSourceRange(), ToSM,
                      FromSM);
  CompareSourceRanges(ToLHS->getSourceRange(), FromLHS->getSourceRange(), ToSM,
                      FromSM);
  CompareSourceRanges(ToRHS->getSourceRange(), FromRHS->getSourceRange(), ToSM,
                      FromSM);
}

TEST_P(ASTImporterTestBase, ImportNestedMacro) {
  Decl *FromTU = getTuDecl(
      R"(
      #define FUNC_INT void declToImport
      #define FUNC FUNC_INT
      FUNC(int a);
      )",
      Lang_CXX);
  auto FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, functionDecl());
  auto ToD = Import(FromD, Lang_CXX);

  SourceManager &ToSM = ToAST->getASTContext().getSourceManager();
  SourceManager &FromSM = FromD->getASTContext().getSourceManager();
  CompareSourceRanges(ToD->getSourceRange(), FromD->getSourceRange(), ToSM,
                      FromSM);
}

TEST_P(
    ASTImporterTestBase,
    ImportDefinitionOfClassTemplateSpecIfThereIsAnExistingFwdDeclAndDefinition)
{
  Decl *ToTU = getToTuDecl(
      R"(
      template <typename T>
      struct B;

      template <>
      struct B<int> {};

      template <>
      struct B<int>;
      )",
      Lang_CXX);
  // We should have only one definition.
  ASSERT_EQ(1u, DeclCounterWithPredicate<ClassTemplateSpecializationDecl>(
                    [](const ClassTemplateSpecializationDecl *T) {
                      return T->isThisDeclarationADefinition();
                    })
                    .match(ToTU, classTemplateSpecializationDecl()));

  Decl *FromTU = getTuDecl(
      R"(
      template <typename T>
      struct B;

      template <>
      struct B<int> {};
      )",
      Lang_CXX, "input1.cc");
  auto *FromD = FirstDeclMatcher<ClassTemplateSpecializationDecl>().match(
      FromTU, classTemplateSpecializationDecl(hasName("B")));

  Import(FromD, Lang_CXX);

  // We should have only one definition.
  EXPECT_EQ(1u, DeclCounterWithPredicate<ClassTemplateSpecializationDecl>(
                    [](const ClassTemplateSpecializationDecl *T) {
                      return T->isThisDeclarationADefinition();
                    })
                    .match(ToTU, classTemplateSpecializationDecl()));
}

TEST_P(ASTImporterTestBase, ObjectsWithUnnamedStructType) {
  Decl *FromTU = getTuDecl(
      R"(
      struct { int a; int b; } object0 = { 2, 3 };
      struct { int x; int y; int z; } object1;
      )",
      Lang_CXX, "input0.cc");

  auto *Obj0 =
      FirstDeclMatcher<VarDecl>().match(FromTU, varDecl(hasName("object0")));
  auto *From0 = getRecordDecl(Obj0);
  auto *Obj1 =
      FirstDeclMatcher<VarDecl>().match(FromTU, varDecl(hasName("object1")));
  auto *From1 = getRecordDecl(Obj1);

  auto *To0 = Import(From0, Lang_CXX);
  auto *To1 = Import(From1, Lang_CXX);

  EXPECT_TRUE(To0);
  EXPECT_TRUE(To1);
  EXPECT_NE(To0, To1);
  EXPECT_NE(To0->getCanonicalDecl(), To1->getCanonicalDecl());
}

TEST_P(ASTImporterTestBase, ImportDoesUpdateUsedFlag) {
  auto Pattern = varDecl(hasName("x"));
  VarDecl *Imported1;
  {
    Decl *FromTU = getTuDecl("extern int x;", Lang_CXX, "input0.cc");
    auto *FromD = FirstDeclMatcher<VarDecl>().match(FromTU, Pattern);
    Imported1 = cast<VarDecl>(Import(FromD, Lang_CXX));
  }
  VarDecl *Imported2;
  {
    Decl *FromTU = getTuDecl("int x;", Lang_CXX, "input1.cc");
    auto *FromD = FirstDeclMatcher<VarDecl>().match(FromTU, Pattern);
    Imported2 = cast<VarDecl>(Import(FromD, Lang_CXX));
  }
  EXPECT_EQ(Imported1->getCanonicalDecl(), Imported2->getCanonicalDecl());
  EXPECT_FALSE(Imported2->isUsed(false));
  {
    Decl *FromTU =
        getTuDecl("extern int x; int f() { return x; }", Lang_CXX, "input2.cc");
    auto *FromD =
        FirstDeclMatcher<FunctionDecl>().match(FromTU, functionDecl());
    Import(FromD, Lang_CXX);
  }
  EXPECT_TRUE(Imported2->isUsed(false));
}

TEST_P(ASTImporterTestBase, ReimportWithUsedFlag) {
  auto Pattern = varDecl(hasName("x"));

  Decl *FromTU = getTuDecl("int x;", Lang_CXX, "input0.cc");
  auto *FromD = FirstDeclMatcher<VarDecl>().match(FromTU, Pattern);

  auto *Imported1 = cast<VarDecl>(Import(FromD, Lang_CXX));

  ASSERT_FALSE(Imported1->isUsed(false));

  FromD->setIsUsed();
  auto *Imported2 = cast<VarDecl>(Import(FromD, Lang_CXX));

  EXPECT_EQ(Imported1, Imported2);
  EXPECT_TRUE(Imported2->isUsed(false));
}

struct ImportFunctions : ASTImporterTestBase {};

TEST_P(ImportFunctions,
       DefinitionShouldBeImportedAsDefintionWhenThereIsAPrototype) {
  Decl *FromTU = getTuDecl("void f(); void f() {}", Lang_CXX);
  auto Pattern = functionDecl(hasName("f"));
  FunctionDecl *FromD = // Definition
      LastDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

  Decl *ImportedD = Import(FromD, Lang_CXX);
  Decl *ToTU = ImportedD->getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  EXPECT_TRUE(cast<FunctionDecl>(ImportedD)->doesThisDeclarationHaveABody());
}

TEST_P(ImportFunctions, DefinitionShouldBeImportedAsADefinition) {
  Decl *FromTU = getTuDecl("void f() {}", Lang_CXX);
  auto Pattern = functionDecl(hasName("f"));
  FunctionDecl *FromD =
      FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

  Decl *ImportedD = Import(FromD, Lang_CXX);
  Decl *ToTU = ImportedD->getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 1u);
  EXPECT_TRUE(cast<FunctionDecl>(ImportedD)->doesThisDeclarationHaveABody());
}

TEST_P(ImportFunctions, ImportPrototypeOfRecursiveFunction) {
  Decl *FromTU = getTuDecl("void f(); void f() { f(); }", Lang_CXX);
  auto Pattern = functionDecl(hasName("f"));
  auto *From =
      FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern); // Proto

  Decl *ImportedD = Import(From, Lang_CXX);
  Decl *ToTU = ImportedD->getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  auto *To0 = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  auto *To1 = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(ImportedD == To0);
  EXPECT_FALSE(To0->doesThisDeclarationHaveABody());
  EXPECT_TRUE(To1->doesThisDeclarationHaveABody());
  EXPECT_EQ(To1->getPreviousDecl(), To0);
}

TEST_P(ImportFunctions, ImportDefinitionOfRecursiveFunction) {
  Decl *FromTU = getTuDecl("void f(); void f() { f(); }", Lang_CXX);
  auto Pattern = functionDecl(hasName("f"));
  auto *From =
      LastDeclMatcher<FunctionDecl>().match(FromTU, Pattern); // Def

  Decl *ImportedD = Import(From, Lang_CXX);
  Decl *ToTU = ImportedD->getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  auto *To0 = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  auto *To1 = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(ImportedD == To1);
  EXPECT_FALSE(To0->doesThisDeclarationHaveABody());
  EXPECT_TRUE(To1->doesThisDeclarationHaveABody());
  EXPECT_EQ(To1->getPreviousDecl(), To0);
}

TEST_P(ImportFunctions, ImportPrototypes) {
  auto Pattern = functionDecl(hasName("f"));

  Decl *ImportedD;
  {
    Decl *FromTU = getTuDecl("void f();", Lang_CXX, "input0.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

    ImportedD = Import(FromD, Lang_CXX);
  }
  {
    Decl *FromTU = getTuDecl("void f();", Lang_CXX, "input1.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    Import(FromD, Lang_CXX);
  }

  Decl *ToTU = ImportedD->getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  auto *To0 = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  auto *To1 = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(ImportedD == To0);
  EXPECT_FALSE(To0->doesThisDeclarationHaveABody());
  EXPECT_FALSE(To1->doesThisDeclarationHaveABody());
  EXPECT_EQ(To1->getPreviousDecl(), To0);
}

TEST_P(ImportFunctions, ImportDefinitions) {
  auto Pattern = functionDecl(hasName("f"));

  Decl *ImportedD;
  {
    Decl *FromTU = getTuDecl("void f(){}", Lang_CXX, "input0.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    ImportedD = Import(FromD, Lang_CXX);
  }
  {
    Decl *FromTU = getTuDecl("void f(){};", Lang_CXX, "input1.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    Import(FromD, Lang_CXX);
  }

  Decl *ToTU = ImportedD->getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 1u);
  auto *To0 = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(ImportedD == To0);
  EXPECT_TRUE(To0->doesThisDeclarationHaveABody());
}

TEST_P(ImportFunctions, ImportDefinitionThenPrototype) {
  auto Pattern = functionDecl(hasName("f"));

  Decl *ImportedD;
  {
    Decl *FromTU = getTuDecl("void f(){}", Lang_CXX, "input0.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    ImportedD = Import(FromD, Lang_CXX);
  }
  {
    Decl *FromTU = getTuDecl("void f();", Lang_CXX, "input1.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    Import(FromD, Lang_CXX);
  }

  Decl *ToTU = ToAST->getASTContext().getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  auto *To0 = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  auto *To1 = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(ImportedD == To0);
  EXPECT_TRUE(To0->doesThisDeclarationHaveABody());
  EXPECT_FALSE(To1->doesThisDeclarationHaveABody());
  EXPECT_EQ(To1->getPreviousDecl(), To0);
}

TEST_P(ImportFunctions, ImportPrototypeThenDefinition) {
  auto Pattern = functionDecl(hasName("f"));

  {
    Decl *FromTU = getTuDecl("void f();", Lang_CXX, "input0.cc");
    FunctionDecl *FromD =
        FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

    Import(FromD, Lang_CXX);
  }
  {
    Decl *FromTU = getTuDecl("void f(){}", Lang_CXX, "input1.cc");
    FunctionDecl *FromD =
        FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    Import(FromD, Lang_CXX);
  }

  Decl *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  ASSERT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  FunctionDecl *ProtoD = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_FALSE(ProtoD->doesThisDeclarationHaveABody());
  FunctionDecl *DefinitionD =
      LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(DefinitionD->doesThisDeclarationHaveABody());
  EXPECT_EQ(DefinitionD->getPreviousDecl(), ProtoD);
}

TEST_P(ImportFunctions, ImportPrototypeThenProtoAndDefinition) {
  auto Pattern = functionDecl(hasName("f"));

  {
    Decl *FromTU = getTuDecl("void f();", Lang_CXX, "input0.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    Import(FromD, Lang_CXX);
  }
  {
    Decl *FromTU = getTuDecl("void f(); void f(){}", Lang_CXX, "input1.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    Import(FromD, Lang_CXX);
  }

  Decl *ToTU = ToAST->getASTContext().getTranslationUnitDecl();

  ASSERT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 3u);
  FunctionDecl *ProtoD = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_FALSE(ProtoD->doesThisDeclarationHaveABody());

  FunctionDecl *DefinitionD =
      LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(DefinitionD->doesThisDeclarationHaveABody());

  EXPECT_TRUE(DefinitionD->getPreviousDecl());
  EXPECT_FALSE(DefinitionD->getPreviousDecl()->doesThisDeclarationHaveABody());
  EXPECT_EQ(DefinitionD->getPreviousDecl()->getPreviousDecl(), ProtoD);
}

TEST_P(ImportFunctions, OverriddenMethodsShouldBeImported) {
  auto Code =
      R"(
      struct B { virtual void f(); };
      void B::f() {}
      struct D : B { void f(); };
      )";
  auto Pattern =
      cxxMethodDecl(hasName("f"), hasParent(cxxRecordDecl(hasName("D"))));
  Decl *FromTU = getTuDecl(Code, Lang_CXX);
  CXXMethodDecl *Proto =
      FirstDeclMatcher<CXXMethodDecl>().match(FromTU, Pattern);

  ASSERT_EQ(Proto->size_overridden_methods(), 1u);
  CXXMethodDecl *To = cast<CXXMethodDecl>(Import(Proto, Lang_CXX));
  EXPECT_EQ(To->size_overridden_methods(), 1u);
}

TEST_P(ImportFunctions, VirtualFlagShouldBePreservedWhenImportingPrototype) {
  auto Code =
      R"(
      struct B { virtual void f(); };
      void B::f() {}
      )";
  auto Pattern =
      cxxMethodDecl(hasName("f"), hasParent(cxxRecordDecl(hasName("B"))));
  Decl *FromTU = getTuDecl(Code, Lang_CXX);
  CXXMethodDecl *Proto =
      FirstDeclMatcher<CXXMethodDecl>().match(FromTU, Pattern);
  CXXMethodDecl *Def = LastDeclMatcher<CXXMethodDecl>().match(FromTU, Pattern);

  ASSERT_TRUE(Proto->isVirtual());
  ASSERT_TRUE(Def->isVirtual());
  CXXMethodDecl *To = cast<CXXMethodDecl>(Import(Proto, Lang_CXX));
  EXPECT_TRUE(To->isVirtual());
}

TEST_P(ImportFunctions,
       ImportDefinitionIfThereIsAnExistingDefinitionAndFwdDecl) {
  Decl *ToTU = getToTuDecl(
      R"(
      void f() {}
      void f();
      )",
      Lang_CXX);
  ASSERT_EQ(1u,
            DeclCounterWithPredicate<FunctionDecl>([](const FunctionDecl *FD) {
              return FD->doesThisDeclarationHaveABody();
            }).match(ToTU, functionDecl()));

  Decl *FromTU = getTuDecl("void f() {}", Lang_CXX, "input0.cc");
  auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, functionDecl());

  Import(FromD, Lang_CXX);

  EXPECT_EQ(1u,
            DeclCounterWithPredicate<FunctionDecl>([](const FunctionDecl *FD) {
              return FD->doesThisDeclarationHaveABody();
            }).match(ToTU, functionDecl()));
}

struct ImportFriendFunctions : ImportFunctions {};

TEST_P(ImportFriendFunctions, ImportFriendFunctionRedeclChainProto) {
  auto Pattern = functionDecl(hasName("f"));

  Decl *FromTU = getTuDecl("struct X { friend void f(); };"
                           "void f();",
                           Lang_CXX,
                           "input0.cc");
  auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

  auto *ImportedD = cast<FunctionDecl>(Import(FromD, Lang_CXX));
  Decl *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  ASSERT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  EXPECT_FALSE(ImportedD->doesThisDeclarationHaveABody());
  auto *ToFD = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_FALSE(ToFD->doesThisDeclarationHaveABody());
  EXPECT_EQ(ToFD->getPreviousDecl(), ImportedD);
}

TEST_P(ImportFriendFunctions,
       ImportFriendFunctionRedeclChainProto_OutOfClassProtoFirst) {
  auto Pattern = functionDecl(hasName("f"));

  Decl *FromTU = getTuDecl("void f();"
                           "struct X { friend void f(); };",
                           Lang_CXX, "input0.cc");
  auto FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

  auto *ImportedD = cast<FunctionDecl>(Import(FromD, Lang_CXX));
  Decl *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  ASSERT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  EXPECT_FALSE(ImportedD->doesThisDeclarationHaveABody());
  auto *ToFD = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_FALSE(ToFD->doesThisDeclarationHaveABody());
  EXPECT_EQ(ToFD->getPreviousDecl(), ImportedD);
}

TEST_P(ImportFriendFunctions, ImportFriendFunctionRedeclChainDef) {
  auto Pattern = functionDecl(hasName("f"));

  Decl *FromTU = getTuDecl("struct X { friend void f(){} };"
                           "void f();",
                           Lang_CXX,
                           "input0.cc");
  auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

  auto *ImportedD = cast<FunctionDecl>(Import(FromD, Lang_CXX));
  Decl *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  ASSERT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  EXPECT_TRUE(ImportedD->doesThisDeclarationHaveABody());
  auto *ToFD = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_FALSE(ToFD->doesThisDeclarationHaveABody());
  EXPECT_EQ(ToFD->getPreviousDecl(), ImportedD);
}

TEST_P(ImportFriendFunctions,
       ImportFriendFunctionRedeclChainDef_OutOfClassDef) {
  auto Pattern = functionDecl(hasName("f"));

  Decl *FromTU = getTuDecl("struct X { friend void f(); };"
                           "void f(){}",
                           Lang_CXX, "input0.cc");
  auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

  auto *ImportedD = cast<FunctionDecl>(Import(FromD, Lang_CXX));
  Decl *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  ASSERT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  EXPECT_FALSE(ImportedD->doesThisDeclarationHaveABody());
  auto *ToFD = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(ToFD->doesThisDeclarationHaveABody());
  EXPECT_EQ(ToFD->getPreviousDecl(), ImportedD);
}

// Disabled temporarily, because the new structural equivalence check
// (https://reviews.llvm.org/D48628) breaks it.
// PreviousDecl is not set because there is no structural match.
// FIXME Enable!
TEST_P(ImportFriendFunctions,
    DISABLED_ImportFriendFunctionRedeclChainDefWithClass) {
  auto Pattern = functionDecl(hasName("f"));

  Decl *FromTU = getTuDecl(
      R"(
        class X;
        void f(X *x){}
        class X{
        friend void f(X *x);
        };
      )",
      Lang_CXX, "input0.cc");
  auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

  auto *ImportedD = cast<FunctionDecl>(Import(FromD, Lang_CXX));
  Decl *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  ASSERT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  EXPECT_TRUE(ImportedD->doesThisDeclarationHaveABody());
  auto *InClassFD = cast<FunctionDecl>(FirstDeclMatcher<FriendDecl>()
                                              .match(ToTU, friendDecl())
                                              ->getFriendDecl());
  EXPECT_FALSE(InClassFD->doesThisDeclarationHaveABody());
  EXPECT_EQ(InClassFD->getPreviousDecl(), ImportedD);
  // The parameters must refer the same type
  EXPECT_EQ((*InClassFD->param_begin())->getOriginalType(),
            (*ImportedD->param_begin())->getOriginalType());
}

// Disabled temporarily, because the new structural equivalence check
// (https://reviews.llvm.org/D48628) breaks it.
// PreviousDecl is not set because there is no structural match.
// FIXME Enable!
TEST_P(ImportFriendFunctions,
    DISABLED_ImportFriendFunctionRedeclChainDefWithClass_ImportTheProto) {
  auto Pattern = functionDecl(hasName("f"));

  Decl *FromTU = getTuDecl(
      R"(
        class X;
        void f(X *x){}
        class X{
        friend void f(X *x);
        };
      )",
      Lang_CXX, "input0.cc");
  auto *FromD = LastDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

  auto *ImportedD = cast<FunctionDecl>(Import(FromD, Lang_CXX));
  Decl *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  ASSERT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  EXPECT_FALSE(ImportedD->doesThisDeclarationHaveABody());
  auto *OutOfClassFD = FirstDeclMatcher<FunctionDecl>().match(
      ToTU, functionDecl(unless(hasParent(friendDecl()))));

  EXPECT_TRUE(OutOfClassFD->doesThisDeclarationHaveABody());
  EXPECT_EQ(ImportedD->getPreviousDecl(), OutOfClassFD);
  // The parameters must refer the same type
  EXPECT_EQ((*OutOfClassFD->param_begin())->getOriginalType(),
            (*ImportedD->param_begin())->getOriginalType());
}

TEST_P(ImportFriendFunctions, ImportFriendFunctionFromMultipleTU) {
  auto Pattern = functionDecl(hasName("f"));

  FunctionDecl *ImportedD;
  {
    Decl *FromTU =
        getTuDecl("struct X { friend void f(){} };", Lang_CXX, "input0.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    ImportedD = cast<FunctionDecl>(Import(FromD, Lang_CXX));
  }
  FunctionDecl *ImportedD1;
  {
    Decl *FromTU = getTuDecl("void f();", Lang_CXX, "input1.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    ImportedD1 = cast<FunctionDecl>(Import(FromD, Lang_CXX));
  }

  Decl *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  ASSERT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  EXPECT_TRUE(ImportedD->doesThisDeclarationHaveABody());
  EXPECT_FALSE(ImportedD1->doesThisDeclarationHaveABody());
  EXPECT_EQ(ImportedD1->getPreviousDecl(), ImportedD);
}

AST_MATCHER_P(TagDecl, hasTypedefForAnonDecl, Matcher<TypedefNameDecl>,
              InnerMatcher) {
  if (auto *Typedef = Node.getTypedefNameForAnonDecl())
    return InnerMatcher.matches(*Typedef, Finder, Builder);
  return false;
}

TEST_P(ImportDecl, ImportEnumSequential) {
  CodeFiles Samples{{"main.c",
                     {"void foo();"
                      "void moo();"
                      "int main() { foo(); moo(); }",
                      Lang_C}},

                    {"foo.c",
                     {"typedef enum { THING_VALUE } thing_t;"
                      "void conflict(thing_t type);"
                      "void foo() { (void)THING_VALUE; }"
                      "void conflict(thing_t type) {}",
                      Lang_C}},

                    {"moo.c",
                     {"typedef enum { THING_VALUE } thing_t;"
                      "void conflict(thing_t type);"
                      "void moo() { conflict(THING_VALUE); }",
                      Lang_C}}};

  auto VerificationMatcher =
      enumDecl(has(enumConstantDecl(hasName("THING_VALUE"))),
               hasTypedefForAnonDecl(hasName("thing_t")));

  ImportAction ImportFoo{"foo.c", "main.c", functionDecl(hasName("foo"))},
      ImportMoo{"moo.c", "main.c", functionDecl(hasName("moo"))};

  testImportSequence(
      Samples, {ImportFoo, ImportMoo}, // "foo", them "moo".
      // Just check that there is only one enum decl in the result AST.
      "main.c", enumDecl(), VerificationMatcher);

  // For different import order, result should be the same.
  testImportSequence(
      Samples, {ImportMoo, ImportFoo}, // "moo", them "foo".
      // Check that there is only one enum decl in the result AST.
      "main.c", enumDecl(), VerificationMatcher);
}

const internal::VariadicDynCastAllOfMatcher<Expr, DependentScopeDeclRefExpr>
    dependentScopeDeclRefExpr;

TEST_P(ImportExpr, DependentScopeDeclRefExpr) {
  MatchVerifier<Decl> Verifier;
  testImport("template <typename T> struct S { static T foo; };"
             "template <typename T> void declToImport() {"
             "  (void) S<T>::foo;"
             "}"
             "void instantiate() { declToImport<int>(); }"
             "template <typename T> T S<T>::foo;",
             Lang_CXX11, "", Lang_CXX11, Verifier,
             functionTemplateDecl(has(functionDecl(has(compoundStmt(
                 has(cStyleCastExpr(has(dependentScopeDeclRefExpr())))))))));

  testImport("template <typename T> struct S {"
             "template<typename S> static void foo(){};"
             "};"
             "template <typename T> void declToImport() {"
             "  S<T>::template foo<T>();"
             "}"
             "void instantiate() { declToImport<int>(); }",
             Lang_CXX11, "", Lang_CXX11, Verifier,
             functionTemplateDecl(has(functionDecl(has(compoundStmt(
                 has(callExpr(has(dependentScopeDeclRefExpr())))))))));
}

const internal::VariadicDynCastAllOfMatcher<Type, DependentNameType>
    dependentNameType;

TEST_P(ImportExpr, DependentNameType) {
  MatchVerifier<Decl> Verifier;
  testImport("template <typename T> struct declToImport {"
             "  typedef typename T::type dependent_name;"
             "};",
             Lang_CXX11, "", Lang_CXX11, Verifier,
             classTemplateDecl(has(
                 cxxRecordDecl(has(typedefDecl(has(dependentNameType())))))));
}

const internal::VariadicDynCastAllOfMatcher<Expr, UnresolvedMemberExpr>
    unresolvedMemberExpr;

TEST_P(ImportExpr, UnresolvedMemberExpr) {
  MatchVerifier<Decl> Verifier;
  testImport("struct S { template <typename T> void mem(); };"
             "template <typename U> void declToImport() {"
             "  S s;"
             "  s.mem<U>();"
             "}"
             "void instantiate() { declToImport<int>(); }",
             Lang_CXX11, "", Lang_CXX11, Verifier,
             functionTemplateDecl(has(functionDecl(has(
                 compoundStmt(has(callExpr(has(unresolvedMemberExpr())))))))));
}

class ImportImplicitMethods : public ASTImporterTestBase {
public:
  static constexpr auto DefaultCode = R"(
      struct A { int x; };
      void f() {
        A a;
        A a1(a);
        A a2(A{});
        a = a1;
        a = A{};
        a.~A();
      })";

  template <typename MatcherType>
  void testImportOf(
      const MatcherType &MethodMatcher, const char *Code = DefaultCode) {
    test(MethodMatcher, Code, /*ExpectedCount=*/1u);
  }

  template <typename MatcherType>
  void testNoImportOf(
      const MatcherType &MethodMatcher, const char *Code = DefaultCode) {
    test(MethodMatcher, Code, /*ExpectedCount=*/0u);
  }

private:
  template <typename MatcherType>
  void test(const MatcherType &MethodMatcher,
      const char *Code, unsigned int ExpectedCount) {
    auto ClassMatcher = cxxRecordDecl(unless(isImplicit()));

    Decl *ToTU = getToTuDecl(Code, Lang_CXX11);
    auto *ToClass = FirstDeclMatcher<CXXRecordDecl>().match(
        ToTU, ClassMatcher);

    ASSERT_EQ(DeclCounter<CXXMethodDecl>().match(ToClass, MethodMatcher), 1u);

    {
      CXXMethodDecl *Method =
          FirstDeclMatcher<CXXMethodDecl>().match(ToClass, MethodMatcher);
      ToClass->removeDecl(Method);
    }

    ASSERT_EQ(DeclCounter<CXXMethodDecl>().match(ToClass, MethodMatcher), 0u);

    Decl *ImportedClass = nullptr;
    {
      Decl *FromTU = getTuDecl(Code, Lang_CXX11, "input1.cc");
      auto *FromClass = FirstDeclMatcher<CXXRecordDecl>().match(
          FromTU, ClassMatcher);
      ImportedClass = Import(FromClass, Lang_CXX11);
    }

    EXPECT_EQ(ToClass, ImportedClass);
    EXPECT_EQ(DeclCounter<CXXMethodDecl>().match(ToClass, MethodMatcher),
        ExpectedCount);
  }
};

TEST_P(ImportImplicitMethods, DefaultConstructor) {
  testImportOf(cxxConstructorDecl(isDefaultConstructor()));
}

TEST_P(ImportImplicitMethods, CopyConstructor) {
  testImportOf(cxxConstructorDecl(isCopyConstructor()));
}

TEST_P(ImportImplicitMethods, MoveConstructor) {
  testImportOf(cxxConstructorDecl(isMoveConstructor()));
}

TEST_P(ImportImplicitMethods, Destructor) {
  testImportOf(cxxDestructorDecl());
}

TEST_P(ImportImplicitMethods, CopyAssignment) {
  testImportOf(cxxMethodDecl(isCopyAssignmentOperator()));
}

TEST_P(ImportImplicitMethods, MoveAssignment) {
  testImportOf(cxxMethodDecl(isMoveAssignmentOperator()));
}

TEST_P(ImportImplicitMethods, DoNotImportUserProvided) {
  auto Code = R"(
      struct A { A() { int x; } };
      )";
  testNoImportOf(cxxConstructorDecl(isDefaultConstructor()), Code);
}

TEST_P(ImportImplicitMethods, DoNotImportDefault) {
  auto Code = R"(
      struct A { A() = default; };
      )";
  testNoImportOf(cxxConstructorDecl(isDefaultConstructor()), Code);
}

TEST_P(ImportImplicitMethods, DoNotImportDeleted) {
  auto Code = R"(
      struct A { A() = delete; };
      )";
  testNoImportOf(cxxConstructorDecl(isDefaultConstructor()), Code);
}

TEST_P(ImportImplicitMethods, DoNotImportOtherMethod) {
  auto Code = R"(
      struct A { void f() { } };
      )";
  testNoImportOf(cxxMethodDecl(hasName("f")), Code);
}

TEST_P(ASTImporterTestBase, ImportOfEquivalentRecord) {
  Decl *ToR1;
  {
    Decl *FromTU = getTuDecl(
        "struct A { };", Lang_CXX, "input0.cc");
    auto *FromR = FirstDeclMatcher<CXXRecordDecl>().match(
        FromTU, cxxRecordDecl(hasName("A")));

    ToR1 = Import(FromR, Lang_CXX);
  }

  Decl *ToR2;
  {
    Decl *FromTU = getTuDecl(
        "struct A { };", Lang_CXX, "input1.cc");
    auto *FromR = FirstDeclMatcher<CXXRecordDecl>().match(
        FromTU, cxxRecordDecl(hasName("A")));

    ToR2 = Import(FromR, Lang_CXX);
  }
  
  EXPECT_EQ(ToR1, ToR2);
}

TEST_P(ASTImporterTestBase, ImportOfNonEquivalentRecord) {
  Decl *ToR1;
  {
    Decl *FromTU = getTuDecl(
        "struct A { int x; };", Lang_CXX, "input0.cc");
    auto *FromR = FirstDeclMatcher<CXXRecordDecl>().match(
        FromTU, cxxRecordDecl(hasName("A")));
    ToR1 = Import(FromR, Lang_CXX);
  }
  Decl *ToR2;
  {
    Decl *FromTU = getTuDecl(
        "struct A { unsigned x; };", Lang_CXX, "input1.cc");
    auto *FromR = FirstDeclMatcher<CXXRecordDecl>().match(
        FromTU, cxxRecordDecl(hasName("A")));
    ToR2 = Import(FromR, Lang_CXX);
  }
  EXPECT_NE(ToR1, ToR2);
}

TEST_P(ASTImporterTestBase, ImportOfEquivalentField) {
  Decl *ToF1;
  {
    Decl *FromTU = getTuDecl(
        "struct A { int x; };", Lang_CXX, "input0.cc");
    auto *FromF = FirstDeclMatcher<FieldDecl>().match(
        FromTU, fieldDecl(hasName("x")));
    ToF1 = Import(FromF, Lang_CXX);
  }
  Decl *ToF2;
  {
    Decl *FromTU = getTuDecl(
        "struct A { int x; };", Lang_CXX, "input1.cc");
    auto *FromF = FirstDeclMatcher<FieldDecl>().match(
        FromTU, fieldDecl(hasName("x")));
    ToF2 = Import(FromF, Lang_CXX);
  }
  EXPECT_EQ(ToF1, ToF2);
}

TEST_P(ASTImporterTestBase, ImportOfNonEquivalentField) {
  Decl *ToF1;
  {
    Decl *FromTU = getTuDecl(
        "struct A { int x; };", Lang_CXX, "input0.cc");
    auto *FromF = FirstDeclMatcher<FieldDecl>().match(
        FromTU, fieldDecl(hasName("x")));
    ToF1 = Import(FromF, Lang_CXX);
  }
  Decl *ToF2;
  {
    Decl *FromTU = getTuDecl(
        "struct A { unsigned x; };", Lang_CXX, "input1.cc");
    auto *FromF = FirstDeclMatcher<FieldDecl>().match(
        FromTU, fieldDecl(hasName("x")));
    ToF2 = Import(FromF, Lang_CXX);
  }
  EXPECT_NE(ToF1, ToF2);
}

TEST_P(ASTImporterTestBase, ImportOfEquivalentMethod) {
  Decl *ToM1;
  {
    Decl *FromTU = getTuDecl(
        "struct A { void x(); }; void A::x() { }", Lang_CXX, "input0.cc");
    auto *FromM = FirstDeclMatcher<FunctionDecl>().match(
        FromTU, functionDecl(hasName("x"), isDefinition()));
    ToM1 = Import(FromM, Lang_CXX);
  }
  Decl *ToM2;
  {
    Decl *FromTU = getTuDecl(
        "struct A { void x(); }; void A::x() { }", Lang_CXX, "input1.cc");
    auto *FromM = FirstDeclMatcher<FunctionDecl>().match(
        FromTU, functionDecl(hasName("x"), isDefinition()));
    ToM2 = Import(FromM, Lang_CXX);
  }
  EXPECT_EQ(ToM1, ToM2);
}

TEST_P(ASTImporterTestBase, ImportOfNonEquivalentMethod) {
  Decl *ToM1;
  {
    Decl *FromTU = getTuDecl(
        "struct A { void x(); }; void A::x() { }",
        Lang_CXX, "input0.cc");
    auto *FromM = FirstDeclMatcher<FunctionDecl>().match(
        FromTU, functionDecl(hasName("x"), isDefinition()));
    ToM1 = Import(FromM, Lang_CXX);
  }
  Decl *ToM2;
  {
    Decl *FromTU = getTuDecl(
        "struct A { void x() const; }; void A::x() const { }",
        Lang_CXX, "input1.cc");
    auto *FromM = FirstDeclMatcher<FunctionDecl>().match(
        FromTU, functionDecl(hasName("x"), isDefinition()));
    ToM2 = Import(FromM, Lang_CXX);
  }
  EXPECT_NE(ToM1, ToM2);
}

TEST_P(ASTImporterTestBase, ImportUnnamedStructsWithRecursingField) {
  Decl *FromTU = getTuDecl(
      R"(
      struct A {
        struct {
          struct A *next;
        } entry0;
        struct {
          struct A *next;
        } entry1;
      };
      )",
      Lang_C, "input0.cc");
  auto *From =
      FirstDeclMatcher<RecordDecl>().match(FromTU, recordDecl(hasName("A")));

  Import(From, Lang_C);

  auto *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  auto *Entry0 =
      FirstDeclMatcher<FieldDecl>().match(ToTU, fieldDecl(hasName("entry0")));
  auto *Entry1 =
      FirstDeclMatcher<FieldDecl>().match(ToTU, fieldDecl(hasName("entry1")));
  auto *R0 = getRecordDecl(Entry0);
  auto *R1 = getRecordDecl(Entry1);
  EXPECT_NE(R0, R1);
  EXPECT_TRUE(MatchVerifier<RecordDecl>().match(
      R0, recordDecl(has(fieldDecl(hasName("next"))))));
  EXPECT_TRUE(MatchVerifier<RecordDecl>().match(
      R1, recordDecl(has(fieldDecl(hasName("next"))))));
}

struct DeclContextTest : ASTImporterTestBase {};

TEST_P(DeclContextTest, removeDeclOfClassTemplateSpecialization) {
  Decl *TU = getTuDecl(
      R"(
      namespace NS {

      template <typename T>
      struct S {};
      template struct S<int>;

      inline namespace INS {
        template <typename T>
        struct S {};
        template struct S<int>;
      }

      }
      )", Lang_CXX11, "input0.cc");
  auto *NS = FirstDeclMatcher<NamespaceDecl>().match(
      TU, namespaceDecl());
  auto *Spec = FirstDeclMatcher<ClassTemplateSpecializationDecl>().match(
      TU, classTemplateSpecializationDecl());
  ASSERT_TRUE(NS->containsDecl(Spec));

  NS->removeDecl(Spec);
  EXPECT_FALSE(NS->containsDecl(Spec));
}

struct ImportFunctionTemplateSpecializations : ASTImporterTestBase {};

TEST_P(ImportFunctionTemplateSpecializations,
       TUshouldNotContainFunctionTemplateImplicitInstantiation) {

  Decl *FromTU = getTuDecl(
      R"(
      template<class T>
      int f() { return 0; }
      void foo() { f<int>(); }
      )",
      Lang_CXX, "input0.cc");

  // Check that the function template instantiation is NOT the child of the TU.
  auto Pattern = translationUnitDecl(
      unless(has(functionDecl(hasName("f"), isTemplateInstantiation()))));
  ASSERT_TRUE(MatchVerifier<Decl>{}.match(FromTU, Pattern));

  auto *Foo = FirstDeclMatcher<FunctionDecl>().match(
      FromTU, functionDecl(hasName("foo")));
  ASSERT_TRUE(Import(Foo, Lang_CXX));

  auto *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  EXPECT_TRUE(MatchVerifier<Decl>{}.match(ToTU, Pattern));
}

TEST_P(ImportFunctionTemplateSpecializations,
       TUshouldNotContainFunctionTemplateExplicitInstantiation) {

  Decl *FromTU = getTuDecl(
      R"(
      template<class T>
      int f() { return 0; }
      template int f<int>();
      )",
      Lang_CXX, "input0.cc");

  // Check that the function template instantiation is NOT the child of the TU.
  auto Instantiation = functionDecl(hasName("f"), isTemplateInstantiation());
  auto Pattern = translationUnitDecl(unless(has(Instantiation)));
  ASSERT_TRUE(MatchVerifier<Decl>{}.match(FromTU, Pattern));

  ASSERT_TRUE(
      Import(FirstDeclMatcher<Decl>().match(FromTU, Instantiation), Lang_CXX));

  auto *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  EXPECT_TRUE(MatchVerifier<Decl>{}.match(ToTU, Pattern));
}

TEST_P(ImportFunctionTemplateSpecializations,
       TUshouldContainFunctionTemplateSpecialization) {

  Decl *FromTU = getTuDecl(
      R"(
      template<class T>
      int f() { return 0; }
      template <> int f<int>() { return 4; }
      )",
      Lang_CXX, "input0.cc");

  // Check that the function template specialization is the child of the TU.
  auto Specialization =
      functionDecl(hasName("f"), isExplicitTemplateSpecialization());
  auto Pattern = translationUnitDecl(has(Specialization));
  ASSERT_TRUE(MatchVerifier<Decl>{}.match(FromTU, Pattern));

  ASSERT_TRUE(
      Import(FirstDeclMatcher<Decl>().match(FromTU, Specialization), Lang_CXX));

  auto *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  EXPECT_TRUE(MatchVerifier<Decl>{}.match(ToTU, Pattern));
}

TEST_P(ImportFunctionTemplateSpecializations,
       FunctionTemplateSpecializationRedeclChain) {

  Decl *FromTU = getTuDecl(
      R"(
      template<class T>
      int f() { return 0; }
      template <> int f<int>() { return 4; }
      )",
      Lang_CXX, "input0.cc");

  auto Spec = functionDecl(hasName("f"), isExplicitTemplateSpecialization(),
                           hasParent(translationUnitDecl()));
  auto *FromSpecD = FirstDeclMatcher<Decl>().match(FromTU, Spec);
  {
    auto *TU = FromTU;
    auto *SpecD = FromSpecD;
    auto *TemplateD = FirstDeclMatcher<FunctionTemplateDecl>().match(
        TU, functionTemplateDecl());
    auto *FirstSpecD = *(TemplateD->spec_begin());
    ASSERT_EQ(SpecD, FirstSpecD);
    ASSERT_TRUE(SpecD->getPreviousDecl());
    ASSERT_FALSE(cast<FunctionDecl>(SpecD->getPreviousDecl())
                     ->doesThisDeclarationHaveABody());
  }

  ASSERT_TRUE(Import(FromSpecD, Lang_CXX));

  {
    auto *TU = ToAST->getASTContext().getTranslationUnitDecl();
    auto *SpecD = FirstDeclMatcher<Decl>().match(TU, Spec);
    auto *TemplateD = FirstDeclMatcher<FunctionTemplateDecl>().match(
        TU, functionTemplateDecl());
    auto *FirstSpecD = *(TemplateD->spec_begin());
    EXPECT_EQ(SpecD, FirstSpecD);
    ASSERT_TRUE(SpecD->getPreviousDecl());
    EXPECT_FALSE(cast<FunctionDecl>(SpecD->getPreviousDecl())
                     ->doesThisDeclarationHaveABody());
  }
}

TEST_P(ImportFunctionTemplateSpecializations,
       MatchNumberOfFunctionTemplateSpecializations) {

  Decl *FromTU = getTuDecl(
      R"(
      template <typename T> constexpr int f() { return 0; }
      template <> constexpr int f<int>() { return 4; }
      void foo() {
        static_assert(f<char>() == 0, "");
        static_assert(f<int>() == 4, "");
      }
      )",
      Lang_CXX11, "input0.cc");
  auto *FromD = FirstDeclMatcher<FunctionDecl>().match(
      FromTU, functionDecl(hasName("foo")));

  Import(FromD, Lang_CXX11);
  auto *ToTU = ToAST->getASTContext().getTranslationUnitDecl();
  EXPECT_EQ(
      DeclCounter<FunctionDecl>().match(FromTU, functionDecl(hasName("f"))),
      DeclCounter<FunctionDecl>().match(ToTU, functionDecl(hasName("f"))));
}

TEST_P(ImportFunctionTemplateSpecializations,
       ImportPrototypes) {
  auto Pattern = functionDecl(hasName("f"), isExplicitTemplateSpecialization());
  auto Code =
      R"(
      // Proto of the primary template.
      template <class T>
      void f();
      // Proto of the specialization.
      template <>
      void f<int>();
      )";

  Decl *ImportedD;
  {
    Decl *FromTU = getTuDecl(Code, Lang_CXX, "input0.cc");
    auto *FromD = LastDeclMatcher<FunctionDecl>().match(FromTU, Pattern);

    ImportedD = Import(FromD, Lang_CXX);
  }
  {
    Decl *FromTU = getTuDecl(Code, Lang_CXX, "input1.cc");
    auto *FromD = LastDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    Import(FromD, Lang_CXX);
  }

  Decl *ToTU = ImportedD->getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  auto *To0 = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  auto *To1 = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(ImportedD == To0);
  EXPECT_TRUE(ImportedD != To1);
  EXPECT_FALSE(To0->doesThisDeclarationHaveABody());
  EXPECT_FALSE(To1->doesThisDeclarationHaveABody());
  // Check that they are part of the same redecl chain.
  EXPECT_EQ(To1->getCanonicalDecl(), To0->getCanonicalDecl());
}

TEST_P(ImportFunctionTemplateSpecializations, ImportDefinitions) {
  auto Pattern = functionDecl(hasName("f"), isExplicitTemplateSpecialization());
  auto Code =
      R"(
      // Proto of the primary template.
      template <class T>
      void f();
      // Specialization and definition.
      template <>
      void f<int>() {}
      )";

  Decl *ImportedD;
  {
    Decl *FromTU = getTuDecl(Code, Lang_CXX, "input0.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    ImportedD = Import(FromD, Lang_CXX);
  }
  {
    Decl *FromTU = getTuDecl(Code, Lang_CXX, "input1.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    Import(FromD, Lang_CXX);
  }

  Decl *ToTU = ImportedD->getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 1u);
  auto *To0 = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(ImportedD == To0);
  EXPECT_TRUE(To0->doesThisDeclarationHaveABody());

  auto *TemplateD = FirstDeclMatcher<FunctionTemplateDecl>().match(
      ToTU, functionTemplateDecl());
  auto *FirstSpecD = *(TemplateD->spec_begin());
  EXPECT_EQ(FirstSpecD->getCanonicalDecl(), To0->getCanonicalDecl());
}

TEST_P(ImportFunctionTemplateSpecializations, PrototypeThenPrototype) {
  auto Pattern = functionDecl(hasName("f"), isExplicitTemplateSpecialization());
  auto Code =
      R"(
      // Proto of the primary template.
      template <class T>
      void f();
      // Specialization proto.
      template <>
      void f<int>();
      // Specialization proto.
      template <>
      void f<int>();
      )";

  Decl *ImportedD;
  {
    Decl *FromTU = getTuDecl(Code, Lang_CXX, "input0.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    ImportedD = Import(FromD, Lang_CXX);
  }

  Decl *ToTU = ImportedD->getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  auto *To0 = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  auto *To1 = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(ImportedD == To0);
  EXPECT_TRUE(ImportedD != To1);
  EXPECT_FALSE(To0->doesThisDeclarationHaveABody());
  EXPECT_FALSE(To1->doesThisDeclarationHaveABody());
  EXPECT_EQ(To1->getPreviousDecl(), To0);
}

TEST_P(ImportFunctionTemplateSpecializations, PrototypeThenDefinition) {
  auto Pattern = functionDecl(hasName("f"), isExplicitTemplateSpecialization());
  auto Code =
      R"(
      // Proto of the primary template.
      template <class T>
      void f();
      // Specialization proto.
      template <>
      void f<int>();
      // Specialization definition.
      template <>
      void f<int>() {}
      )";

  Decl *ImportedD;
  {
    Decl *FromTU = getTuDecl(Code, Lang_CXX, "input0.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    ImportedD = Import(FromD, Lang_CXX);
  }

  Decl *ToTU = ImportedD->getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  auto *To0 = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  auto *To1 = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(ImportedD == To0);
  EXPECT_TRUE(ImportedD != To1);
  EXPECT_FALSE(To0->doesThisDeclarationHaveABody());
  EXPECT_TRUE(To1->doesThisDeclarationHaveABody());
  EXPECT_EQ(To1->getPreviousDecl(), To0);
}

TEST_P(ImportFunctionTemplateSpecializations, DefinitionThenPrototype) {
  auto Pattern = functionDecl(hasName("f"), isExplicitTemplateSpecialization());
  auto Code =
      R"(
      // Proto of the primary template.
      template <class T>
      void f();
      // Specialization definition.
      template <>
      void f<int>() {}
      // Specialization proto.
      template <>
      void f<int>();
      )";

  Decl *ImportedD;
  {
    Decl *FromTU = getTuDecl(Code, Lang_CXX, "input0.cc");
    auto *FromD = FirstDeclMatcher<FunctionDecl>().match(FromTU, Pattern);
    ImportedD = Import(FromD, Lang_CXX);
  }

  Decl *ToTU = ImportedD->getTranslationUnitDecl();

  EXPECT_EQ(DeclCounter<FunctionDecl>().match(ToTU, Pattern), 2u);
  auto *To0 = FirstDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  auto *To1 = LastDeclMatcher<FunctionDecl>().match(ToTU, Pattern);
  EXPECT_TRUE(ImportedD == To0);
  EXPECT_TRUE(ImportedD != To1);
  EXPECT_TRUE(To0->doesThisDeclarationHaveABody());
  EXPECT_FALSE(To1->doesThisDeclarationHaveABody());
  EXPECT_EQ(To1->getPreviousDecl(), To0);
}

INSTANTIATE_TEST_CASE_P(ParameterizedTests, DeclContextTest,
                        ::testing::Values(ArgVector()), );

INSTANTIATE_TEST_CASE_P(
    ParameterizedTests, CanonicalRedeclChain,
    ::testing::Values(ArgVector()),);

auto DefaultTestValuesForRunOptions = ::testing::Values(
    ArgVector(),
    ArgVector{"-fdelayed-template-parsing"},
    ArgVector{"-fms-compatibility"},
    ArgVector{"-fdelayed-template-parsing", "-fms-compatibility"});

INSTANTIATE_TEST_CASE_P(ParameterizedTests, ImportExpr,
                        DefaultTestValuesForRunOptions, );

INSTANTIATE_TEST_CASE_P(ParameterizedTests, ImportType,
                        DefaultTestValuesForRunOptions, );

INSTANTIATE_TEST_CASE_P(ParameterizedTests, ImportDecl,
                        DefaultTestValuesForRunOptions, );

INSTANTIATE_TEST_CASE_P(ParameterizedTests, ASTImporterTestBase,
                        DefaultTestValuesForRunOptions, );

INSTANTIATE_TEST_CASE_P(ParameterizedTests, ImportFunctions,
                        DefaultTestValuesForRunOptions, );

INSTANTIATE_TEST_CASE_P(ParameterizedTests, ImportFriendFunctions,
                        DefaultTestValuesForRunOptions, );

INSTANTIATE_TEST_CASE_P(ParameterizedTests,
                        ImportFunctionTemplateSpecializations,
                        DefaultTestValuesForRunOptions, );

} // end namespace ast_matchers
} // end namespace clang
