//===- unittests/Lex/HeaderSearchTest.cpp ------ HeaderSearch tests -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/Lex/HeaderSearch.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/MemoryBufferCache.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "gtest/gtest.h"

namespace clang {
namespace {

// The test fixture.
class HeaderSearchTest : public ::testing::Test {
protected:
  HeaderSearchTest()
      : VFS(new vfs::InMemoryFileSystem), FileMgr(FileMgrOpts, VFS),
        DiagID(new DiagnosticIDs()),
        Diags(DiagID, new DiagnosticOptions, new IgnoringDiagConsumer()),
        SourceMgr(Diags, FileMgr), TargetOpts(new TargetOptions),
        Search(std::make_shared<HeaderSearchOptions>(), SourceMgr, Diags,
               LangOpts, Target.get()) {
    TargetOpts->Triple = "x86_64-apple-darwin11.1.0";
    Target = TargetInfo::CreateTargetInfo(Diags, TargetOpts);
  }

  void addSearchDir(llvm::StringRef Dir) {
    VFS->addFile(Dir, 0, llvm::MemoryBuffer::getMemBuffer(""), /*User=*/None,
                 /*Group=*/None, llvm::sys::fs::file_type::directory_file);
    const DirectoryEntry *DE = FileMgr.getDirectory(Dir);
    assert(DE);
    auto DL = DirectoryLookup(DE, SrcMgr::C_User, /*isFramework=*/false);
    Search.AddSearchPath(DL, /*isAngled=*/false);
  }

  IntrusiveRefCntPtr<vfs::InMemoryFileSystem> VFS;
  FileSystemOptions FileMgrOpts;
  FileManager FileMgr;
  IntrusiveRefCntPtr<DiagnosticIDs> DiagID;
  DiagnosticsEngine Diags;
  SourceManager SourceMgr;
  LangOptions LangOpts;
  std::shared_ptr<TargetOptions> TargetOpts;
  IntrusiveRefCntPtr<TargetInfo> Target;
  HeaderSearch Search;
};

TEST_F(HeaderSearchTest, NoSearchDir) {
  EXPECT_EQ(Search.search_dir_size(), 0u);
  EXPECT_EQ(Search.suggestPathToFileForDiagnostics("/x/y/z", /*WorkingDir=*/""),
            "/x/y/z");
}

TEST_F(HeaderSearchTest, SimpleShorten) {
  addSearchDir("/x");
  addSearchDir("/x/y");
  EXPECT_EQ(Search.suggestPathToFileForDiagnostics("/x/y/z", /*WorkingDir=*/""),
            "z");
  addSearchDir("/a/b/");
  EXPECT_EQ(Search.suggestPathToFileForDiagnostics("/a/b/c", /*WorkingDir=*/""),
            "c");
}

TEST_F(HeaderSearchTest, ShortenWithWorkingDir) {
  addSearchDir("x/y");
  EXPECT_EQ(Search.suggestPathToFileForDiagnostics("/a/b/c/x/y/z",
                                                   /*WorkingDir=*/"/a/b/c"),
            "z");
}

TEST_F(HeaderSearchTest, Dots) {
  addSearchDir("/x/./y/");
  EXPECT_EQ(Search.suggestPathToFileForDiagnostics("/x/y/./z",
                                                   /*WorkingDir=*/""),
            "z");
  addSearchDir("a/.././c/");
  EXPECT_EQ(Search.suggestPathToFileForDiagnostics("/m/n/./c/z",
                                                   /*WorkingDir=*/"/m/n/"),
            "z");
}

} // namespace
} // namespace clang
