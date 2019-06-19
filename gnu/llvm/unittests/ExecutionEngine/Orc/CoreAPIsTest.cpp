//===----------- CoreAPIsTest.cpp - Unit tests for Core ORC APIs ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "OrcTestCommon.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/OrcError.h"

#include <set>
#include <thread>

using namespace llvm;
using namespace llvm::orc;

class CoreAPIsStandardTest : public CoreAPIsBasedStandardTest {};

namespace {

class SimpleMaterializationUnit : public MaterializationUnit {
public:
  using MaterializeFunction =
      std::function<void(MaterializationResponsibility)>;
  using DiscardFunction = std::function<void(const VSO &, SymbolStringPtr)>;
  using DestructorFunction = std::function<void()>;

  SimpleMaterializationUnit(
      SymbolFlagsMap SymbolFlags, MaterializeFunction Materialize,
      DiscardFunction Discard = DiscardFunction(),
      DestructorFunction Destructor = DestructorFunction())
      : MaterializationUnit(std::move(SymbolFlags)),
        Materialize(std::move(Materialize)), Discard(std::move(Discard)),
        Destructor(std::move(Destructor)) {}

  ~SimpleMaterializationUnit() override {
    if (Destructor)
      Destructor();
  }

  void materialize(MaterializationResponsibility R) override {
    Materialize(std::move(R));
  }

  void discard(const VSO &V, SymbolStringPtr Name) override {
    if (Discard)
      Discard(V, std::move(Name));
    else
      llvm_unreachable("Discard not supported");
  }

private:
  MaterializeFunction Materialize;
  DiscardFunction Discard;
  DestructorFunction Destructor;
};

TEST_F(CoreAPIsStandardTest, BasicSuccessfulLookup) {
  bool OnResolutionRun = false;
  bool OnReadyRun = false;

  auto OnResolution = [&](Expected<SymbolMap> Result) {
    EXPECT_TRUE(!!Result) << "Resolution unexpectedly returned error";
    auto &Resolved = *Result;
    auto I = Resolved.find(Foo);
    EXPECT_NE(I, Resolved.end()) << "Could not find symbol definition";
    EXPECT_EQ(I->second.getAddress(), FooAddr)
        << "Resolution returned incorrect result";
    OnResolutionRun = true;
  };
  auto OnReady = [&](Error Err) {
    cantFail(std::move(Err));
    OnReadyRun = true;
  };

  std::shared_ptr<MaterializationResponsibility> FooMR;

  cantFail(V.define(llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Foo, FooSym.getFlags()}}),
      [&](MaterializationResponsibility R) {
        FooMR = std::make_shared<MaterializationResponsibility>(std::move(R));
      })));

  ES.lookup({&V}, {Foo}, OnResolution, OnReady, NoDependenciesToRegister);

  EXPECT_FALSE(OnResolutionRun) << "Should not have been resolved yet";
  EXPECT_FALSE(OnReadyRun) << "Should not have been marked ready yet";

  FooMR->resolve({{Foo, FooSym}});

  EXPECT_TRUE(OnResolutionRun) << "Should have been resolved";
  EXPECT_FALSE(OnReadyRun) << "Should not have been marked ready yet";

  FooMR->finalize();

  EXPECT_TRUE(OnReadyRun) << "Should have been marked ready";
}

TEST_F(CoreAPIsStandardTest, ExecutionSessionFailQuery) {
  bool OnResolutionRun = false;
  bool OnReadyRun = false;

  auto OnResolution = [&](Expected<SymbolMap> Result) {
    EXPECT_FALSE(!!Result) << "Resolution unexpectedly returned success";
    auto Msg = toString(Result.takeError());
    EXPECT_EQ(Msg, "xyz") << "Resolution returned incorrect result";
    OnResolutionRun = true;
  };
  auto OnReady = [&](Error Err) {
    cantFail(std::move(Err));
    OnReadyRun = true;
  };

  AsynchronousSymbolQuery Q(SymbolNameSet({Foo}), OnResolution, OnReady);

  ES.legacyFailQuery(Q,
                     make_error<StringError>("xyz", inconvertibleErrorCode()));

  EXPECT_TRUE(OnResolutionRun) << "OnResolutionCallback was not run";
  EXPECT_FALSE(OnReadyRun) << "OnReady unexpectedly run";
}

TEST_F(CoreAPIsStandardTest, EmptyLookup) {
  bool OnResolvedRun = false;
  bool OnReadyRun = false;

  auto OnResolution = [&](Expected<SymbolMap> Result) {
    cantFail(std::move(Result));
    OnResolvedRun = true;
  };

  auto OnReady = [&](Error Err) {
    cantFail(std::move(Err));
    OnReadyRun = true;
  };

  ES.lookup({&V}, {}, OnResolution, OnReady, NoDependenciesToRegister);

  EXPECT_TRUE(OnResolvedRun) << "OnResolved was not run for empty query";
  EXPECT_TRUE(OnReadyRun) << "OnReady was not run for empty query";
}

TEST_F(CoreAPIsStandardTest, ChainedVSOLookup) {
  cantFail(V.define(absoluteSymbols({{Foo, FooSym}})));

  auto &V2 = ES.createVSO("V2");

  bool OnResolvedRun = false;
  bool OnReadyRun = false;

  auto Q = std::make_shared<AsynchronousSymbolQuery>(
      SymbolNameSet({Foo}),
      [&](Expected<SymbolMap> Result) {
        cantFail(std::move(Result));
        OnResolvedRun = true;
      },
      [&](Error Err) {
        cantFail(std::move(Err));
        OnReadyRun = true;
      });

  V2.legacyLookup(Q, V.legacyLookup(Q, {Foo}));

  EXPECT_TRUE(OnResolvedRun) << "OnResolved was not run for empty query";
  EXPECT_TRUE(OnReadyRun) << "OnReady was not run for empty query";
}

TEST_F(CoreAPIsStandardTest, LookupFlagsTest) {
  // Test that lookupFlags works on a predefined symbol, and does not trigger
  // materialization of a lazy symbol. Make the lazy symbol weak to test that
  // the weak flag is propagated correctly.

  BarSym.setFlags(static_cast<JITSymbolFlags::FlagNames>(
      JITSymbolFlags::Exported | JITSymbolFlags::Weak));
  auto MU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Bar, BarSym.getFlags()}}),
      [](MaterializationResponsibility R) {
        llvm_unreachable("Symbol materialized on flags lookup");
      });

  cantFail(V.define(absoluteSymbols({{Foo, FooSym}})));
  cantFail(V.define(std::move(MU)));

  SymbolNameSet Names({Foo, Bar, Baz});

  auto SymbolFlags = V.lookupFlags(Names);

  EXPECT_EQ(SymbolFlags.size(), 2U)
      << "Returned symbol flags contains unexpected results";
  EXPECT_EQ(SymbolFlags.count(Foo), 1U) << "Missing lookupFlags result for Foo";
  EXPECT_EQ(SymbolFlags[Foo], FooSym.getFlags())
      << "Incorrect flags returned for Foo";
  EXPECT_EQ(SymbolFlags.count(Bar), 1U)
      << "Missing  lookupFlags result for Bar";
  EXPECT_EQ(SymbolFlags[Bar], BarSym.getFlags())
      << "Incorrect flags returned for Bar";
}

TEST_F(CoreAPIsStandardTest, TestBasicAliases) {
  cantFail(V.define(absoluteSymbols({{Foo, FooSym}, {Bar, BarSym}})));
  cantFail(V.define(symbolAliases({{Baz, {Foo, JITSymbolFlags::Exported}},
                                   {Qux, {Bar, JITSymbolFlags::Weak}}})));
  cantFail(V.define(absoluteSymbols({{Qux, QuxSym}})));

  auto Result = lookup({&V}, {Baz, Qux});
  EXPECT_TRUE(!!Result) << "Unexpected lookup failure";
  EXPECT_EQ(Result->count(Baz), 1U) << "No result for \"baz\"";
  EXPECT_EQ(Result->count(Qux), 1U) << "No result for \"qux\"";
  EXPECT_EQ((*Result)[Baz].getAddress(), FooSym.getAddress())
      << "\"Baz\"'s address should match \"Foo\"'s";
  EXPECT_EQ((*Result)[Qux].getAddress(), QuxSym.getAddress())
      << "The \"Qux\" alias should have been overriden";
}

TEST_F(CoreAPIsStandardTest, TestChainedAliases) {
  cantFail(V.define(absoluteSymbols({{Foo, FooSym}})));
  cantFail(V.define(symbolAliases(
      {{Baz, {Bar, BazSym.getFlags()}}, {Bar, {Foo, BarSym.getFlags()}}})));

  auto Result = lookup({&V}, {Bar, Baz});
  EXPECT_TRUE(!!Result) << "Unexpected lookup failure";
  EXPECT_EQ(Result->count(Bar), 1U) << "No result for \"bar\"";
  EXPECT_EQ(Result->count(Baz), 1U) << "No result for \"baz\"";
  EXPECT_EQ((*Result)[Bar].getAddress(), FooSym.getAddress())
      << "\"Bar\"'s address should match \"Foo\"'s";
  EXPECT_EQ((*Result)[Baz].getAddress(), FooSym.getAddress())
      << "\"Baz\"'s address should match \"Foo\"'s";
}

TEST_F(CoreAPIsStandardTest, TestBasicReExports) {
  // Test that the basic use case of re-exporting a single symbol from another
  // VSO works.
  cantFail(V.define(absoluteSymbols({{Foo, FooSym}})));

  auto &V2 = ES.createVSO("V2");

  cantFail(V2.define(reexports(V, {{Bar, {Foo, BarSym.getFlags()}}})));

  auto Result = cantFail(lookup({&V2}, Bar));
  EXPECT_EQ(Result.getAddress(), FooSym.getAddress())
      << "Re-export Bar for symbol Foo should match FooSym's address";
}

TEST_F(CoreAPIsStandardTest, TestThatReExportsDontUnnecessarilyMaterialize) {
  // Test that re-exports do not materialize symbols that have not been queried
  // for.
  cantFail(V.define(absoluteSymbols({{Foo, FooSym}})));

  bool BarMaterialized = false;
  auto BarMU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Bar, BarSym.getFlags()}}),
      [&](MaterializationResponsibility R) {
        BarMaterialized = true;
        R.resolve({{Bar, BarSym}});
        R.finalize();
      });

  cantFail(V.define(BarMU));

  auto &V2 = ES.createVSO("V2");

  cantFail(V2.define(reexports(
      V, {{Baz, {Foo, BazSym.getFlags()}}, {Qux, {Bar, QuxSym.getFlags()}}})));

  auto Result = cantFail(lookup({&V2}, Baz));
  EXPECT_EQ(Result.getAddress(), FooSym.getAddress())
      << "Re-export Baz for symbol Foo should match FooSym's address";

  EXPECT_FALSE(BarMaterialized) << "Bar should not have been materialized";
}

TEST_F(CoreAPIsStandardTest, TestTrivialCircularDependency) {
  Optional<MaterializationResponsibility> FooR;
  auto FooMU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Foo, FooSym.getFlags()}}),
      [&](MaterializationResponsibility R) { FooR.emplace(std::move(R)); });

  cantFail(V.define(FooMU));

  bool FooReady = false;
  auto OnResolution = [](Expected<SymbolMap> R) { cantFail(std::move(R)); };
  auto OnReady = [&](Error Err) {
    cantFail(std::move(Err));
    FooReady = true;
  };

  ES.lookup({&V}, {Foo}, std::move(OnResolution), std::move(OnReady),
            NoDependenciesToRegister);

  FooR->resolve({{Foo, FooSym}});
  FooR->finalize();

  EXPECT_TRUE(FooReady)
    << "Self-dependency prevented symbol from being marked ready";
}

TEST_F(CoreAPIsStandardTest, TestCircularDependenceInOneVSO) {
  // Test that a circular symbol dependency between three symbols in a VSO does
  // not prevent any symbol from becoming 'ready' once all symbols are
  // finalized.

  // Create three MaterializationResponsibility objects: one for each of Foo,
  // Bar and Baz. These are optional because MaterializationResponsibility
  // does not have a default constructor).
  Optional<MaterializationResponsibility> FooR;
  Optional<MaterializationResponsibility> BarR;
  Optional<MaterializationResponsibility> BazR;

  // Create a MaterializationUnit for each symbol that moves the
  // MaterializationResponsibility into one of the locals above.
  auto FooMU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Foo, FooSym.getFlags()}}),
      [&](MaterializationResponsibility R) { FooR.emplace(std::move(R)); });

  auto BarMU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Bar, BarSym.getFlags()}}),
      [&](MaterializationResponsibility R) { BarR.emplace(std::move(R)); });

  auto BazMU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Baz, BazSym.getFlags()}}),
      [&](MaterializationResponsibility R) { BazR.emplace(std::move(R)); });

  // Define the symbols.
  cantFail(V.define(FooMU));
  cantFail(V.define(BarMU));
  cantFail(V.define(BazMU));

  // Query each of the symbols to trigger materialization.
  bool FooResolved = false;
  bool FooReady = false;

  auto OnFooResolution = [&](Expected<SymbolMap> Result) {
    cantFail(std::move(Result));
    FooResolved = true;
  };

  auto OnFooReady = [&](Error Err) {
    cantFail(std::move(Err));
    FooReady = true;
  };

  // Issue a lookup for Foo. Use NoDependenciesToRegister: We're going to add
  // the dependencies manually below.
  ES.lookup({&V}, {Foo}, std::move(OnFooResolution), std::move(OnFooReady),
            NoDependenciesToRegister);

  bool BarResolved = false;
  bool BarReady = false;
  auto OnBarResolution = [&](Expected<SymbolMap> Result) {
    cantFail(std::move(Result));
    BarResolved = true;
  };

  auto OnBarReady = [&](Error Err) {
    cantFail(std::move(Err));
    BarReady = true;
  };

  ES.lookup({&V}, {Bar}, std::move(OnBarResolution), std::move(OnBarReady),
            NoDependenciesToRegister);

  bool BazResolved = false;
  bool BazReady = false;

  auto OnBazResolution = [&](Expected<SymbolMap> Result) {
    cantFail(std::move(Result));
    BazResolved = true;
  };

  auto OnBazReady = [&](Error Err) {
    cantFail(std::move(Err));
    BazReady = true;
  };

  ES.lookup({&V}, {Baz}, std::move(OnBazResolution), std::move(OnBazReady),
            NoDependenciesToRegister);

  // Add a circular dependency: Foo -> Bar, Bar -> Baz, Baz -> Foo.
  FooR->addDependenciesForAll({{&V, SymbolNameSet({Bar})}});
  BarR->addDependenciesForAll({{&V, SymbolNameSet({Baz})}});
  BazR->addDependenciesForAll({{&V, SymbolNameSet({Foo})}});

  // Add self-dependencies for good measure. This tests that the implementation
  // of addDependencies filters these out.
  FooR->addDependenciesForAll({{&V, SymbolNameSet({Foo})}});
  BarR->addDependenciesForAll({{&V, SymbolNameSet({Bar})}});
  BazR->addDependenciesForAll({{&V, SymbolNameSet({Baz})}});

  // Check that nothing has been resolved yet.
  EXPECT_FALSE(FooResolved) << "\"Foo\" should not be resolved yet";
  EXPECT_FALSE(BarResolved) << "\"Bar\" should not be resolved yet";
  EXPECT_FALSE(BazResolved) << "\"Baz\" should not be resolved yet";

  // Resolve the symbols (but do not finalized them).
  FooR->resolve({{Foo, FooSym}});
  BarR->resolve({{Bar, BarSym}});
  BazR->resolve({{Baz, BazSym}});

  // Verify that the symbols have been resolved, but are not ready yet.
  EXPECT_TRUE(FooResolved) << "\"Foo\" should be resolved now";
  EXPECT_TRUE(BarResolved) << "\"Bar\" should be resolved now";
  EXPECT_TRUE(BazResolved) << "\"Baz\" should be resolved now";

  EXPECT_FALSE(FooReady) << "\"Foo\" should not be ready yet";
  EXPECT_FALSE(BarReady) << "\"Bar\" should not be ready yet";
  EXPECT_FALSE(BazReady) << "\"Baz\" should not be ready yet";

  // Finalize two of the symbols.
  FooR->finalize();
  BarR->finalize();

  // Verify that nothing is ready until the circular dependence is resolved.
  EXPECT_FALSE(FooReady) << "\"Foo\" still should not be ready";
  EXPECT_FALSE(BarReady) << "\"Bar\" still should not be ready";
  EXPECT_FALSE(BazReady) << "\"Baz\" still should not be ready";

  // Finalize the last symbol.
  BazR->finalize();

  // Verify that everything becomes ready once the circular dependence resolved.
  EXPECT_TRUE(FooReady) << "\"Foo\" should be ready now";
  EXPECT_TRUE(BarReady) << "\"Bar\" should be ready now";
  EXPECT_TRUE(BazReady) << "\"Baz\" should be ready now";
}

TEST_F(CoreAPIsStandardTest, DropMaterializerWhenEmpty) {
  bool DestructorRun = false;

  JITSymbolFlags WeakExported(JITSymbolFlags::Exported);
  WeakExported |= JITSymbolFlags::Weak;

  auto MU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Foo, WeakExported}, {Bar, WeakExported}}),
      [](MaterializationResponsibility R) {
        llvm_unreachable("Unexpected call to materialize");
      },
      [&](const VSO &V, SymbolStringPtr Name) {
        EXPECT_TRUE(Name == Foo || Name == Bar)
            << "Discard of unexpected symbol?";
      },
      [&]() { DestructorRun = true; });

  cantFail(V.define(MU));

  cantFail(V.define(absoluteSymbols({{Foo, FooSym}})));

  EXPECT_FALSE(DestructorRun)
      << "MaterializationUnit should not have been destroyed yet";

  cantFail(V.define(absoluteSymbols({{Bar, BarSym}})));

  EXPECT_TRUE(DestructorRun)
      << "MaterializationUnit should have been destroyed";
}

TEST_F(CoreAPIsStandardTest, AddAndMaterializeLazySymbol) {
  bool FooMaterialized = false;
  bool BarDiscarded = false;

  JITSymbolFlags WeakExported(JITSymbolFlags::Exported);
  WeakExported |= JITSymbolFlags::Weak;

  auto MU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Foo, JITSymbolFlags::Exported}, {Bar, WeakExported}}),
      [&](MaterializationResponsibility R) {
        assert(BarDiscarded && "Bar should have been discarded by this point");
        R.resolve(SymbolMap({{Foo, FooSym}}));
        R.finalize();
        FooMaterialized = true;
      },
      [&](const VSO &V, SymbolStringPtr Name) {
        EXPECT_EQ(Name, Bar) << "Expected Name to be Bar";
        BarDiscarded = true;
      });

  cantFail(V.define(MU));
  cantFail(V.define(absoluteSymbols({{Bar, BarSym}})));

  SymbolNameSet Names({Foo});

  bool OnResolutionRun = false;
  bool OnReadyRun = false;

  auto OnResolution = [&](Expected<SymbolMap> Result) {
    EXPECT_TRUE(!!Result) << "Resolution unexpectedly returned error";
    auto I = Result->find(Foo);
    EXPECT_NE(I, Result->end()) << "Could not find symbol definition";
    EXPECT_EQ(I->second.getAddress(), FooSym.getAddress())
        << "Resolution returned incorrect result";
    OnResolutionRun = true;
  };

  auto OnReady = [&](Error Err) {
    cantFail(std::move(Err));
    OnReadyRun = true;
  };

  ES.lookup({&V}, Names, std::move(OnResolution), std::move(OnReady),
            NoDependenciesToRegister);

  EXPECT_TRUE(FooMaterialized) << "Foo was not materialized";
  EXPECT_TRUE(BarDiscarded) << "Bar was not discarded";
  EXPECT_TRUE(OnResolutionRun) << "OnResolutionCallback was not run";
  EXPECT_TRUE(OnReadyRun) << "OnReady was not run";
}

TEST_F(CoreAPIsStandardTest, DefineMaterializingSymbol) {
  bool ExpectNoMoreMaterialization = false;
  ES.setDispatchMaterialization(
      [&](VSO &V, std::unique_ptr<MaterializationUnit> MU) {
        if (ExpectNoMoreMaterialization)
          ADD_FAILURE() << "Unexpected materialization";
        MU->doMaterialize(V);
      });

  auto MU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Foo, FooSym.getFlags()}}),
      [&](MaterializationResponsibility R) {
        cantFail(
            R.defineMaterializing(SymbolFlagsMap({{Bar, BarSym.getFlags()}})));
        R.resolve(SymbolMap({{Foo, FooSym}, {Bar, BarSym}}));
        R.finalize();
      });

  cantFail(V.define(MU));
  cantFail(lookup({&V}, Foo));

  // Assert that materialization is complete by now.
  ExpectNoMoreMaterialization = true;

  // Look up bar to verify that no further materialization happens.
  auto BarResult = cantFail(lookup({&V}, Bar));
  EXPECT_EQ(BarResult.getAddress(), BarSym.getAddress())
      << "Expected Bar == BarSym";
}

TEST_F(CoreAPIsStandardTest, FallbackDefinitionGeneratorTest) {
  cantFail(V.define(absoluteSymbols({{Foo, FooSym}})));

  V.setFallbackDefinitionGenerator([&](VSO &W, const SymbolNameSet &Names) {
    cantFail(W.define(absoluteSymbols({{Bar, BarSym}})));
    return SymbolNameSet({Bar});
  });

  auto Result = cantFail(lookup({&V}, {Foo, Bar}));

  EXPECT_EQ(Result.count(Bar), 1U) << "Expected to find fallback def for 'bar'";
  EXPECT_EQ(Result[Bar].getAddress(), BarSym.getAddress())
      << "Expected fallback def for Bar to be equal to BarSym";
}

TEST_F(CoreAPIsStandardTest, FailResolution) {
  auto MU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap(
          {{Foo, JITSymbolFlags::Weak}, {Bar, JITSymbolFlags::Weak}}),
      [&](MaterializationResponsibility R) { R.failMaterialization(); });

  cantFail(V.define(MU));

  SymbolNameSet Names({Foo, Bar});
  auto Result = lookup({&V}, Names);

  EXPECT_FALSE(!!Result) << "Expected failure";
  if (!Result) {
    handleAllErrors(Result.takeError(),
                    [&](FailedToMaterialize &F) {
                      EXPECT_EQ(F.getSymbols(), Names)
                          << "Expected to fail on symbols in Names";
                    },
                    [](ErrorInfoBase &EIB) {
                      std::string ErrMsg;
                      {
                        raw_string_ostream ErrOut(ErrMsg);
                        EIB.log(ErrOut);
                      }
                      ADD_FAILURE()
                          << "Expected a FailedToResolve error. Got:\n"
                          << ErrMsg;
                    });
  }
}

TEST_F(CoreAPIsStandardTest, TestLookupWithUnthreadedMaterialization) {
  auto MU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Foo, JITSymbolFlags::Exported}}),
      [&](MaterializationResponsibility R) {
        R.resolve({{Foo, FooSym}});
        R.finalize();
      });

  cantFail(V.define(MU));

  auto FooLookupResult = cantFail(lookup({&V}, Foo));

  EXPECT_EQ(FooLookupResult.getAddress(), FooSym.getAddress())
      << "lookup returned an incorrect address";
  EXPECT_EQ(FooLookupResult.getFlags(), FooSym.getFlags())
      << "lookup returned incorrect flags";
}

TEST_F(CoreAPIsStandardTest, TestLookupWithThreadedMaterialization) {
#if LLVM_ENABLE_THREADS

  std::thread MaterializationThread;
  ES.setDispatchMaterialization(
      [&](VSO &V, std::unique_ptr<MaterializationUnit> MU) {
        auto SharedMU = std::shared_ptr<MaterializationUnit>(std::move(MU));
        MaterializationThread =
            std::thread([SharedMU, &V]() { SharedMU->doMaterialize(V); });
      });

  cantFail(V.define(absoluteSymbols({{Foo, FooSym}})));

  auto FooLookupResult = cantFail(lookup({&V}, Foo));

  EXPECT_EQ(FooLookupResult.getAddress(), FooSym.getAddress())
      << "lookup returned an incorrect address";
  EXPECT_EQ(FooLookupResult.getFlags(), FooSym.getFlags())
      << "lookup returned incorrect flags";
  MaterializationThread.join();
#endif
}

TEST_F(CoreAPIsStandardTest, TestGetRequestedSymbolsAndReplace) {
  // Test that GetRequestedSymbols returns the set of symbols that currently
  // have pending queries, and test that MaterializationResponsibility's
  // replace method can be used to return definitions to the VSO in a new
  // MaterializationUnit.
  SymbolNameSet Names({Foo, Bar});

  bool FooMaterialized = false;
  bool BarMaterialized = false;

  auto MU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Foo, FooSym.getFlags()}, {Bar, BarSym.getFlags()}}),
      [&](MaterializationResponsibility R) {
        auto Requested = R.getRequestedSymbols();
        EXPECT_EQ(Requested.size(), 1U) << "Expected one symbol requested";
        EXPECT_EQ(*Requested.begin(), Foo) << "Expected \"Foo\" requested";

        auto NewMU = llvm::make_unique<SimpleMaterializationUnit>(
            SymbolFlagsMap({{Bar, BarSym.getFlags()}}),
            [&](MaterializationResponsibility R2) {
              R2.resolve(SymbolMap({{Bar, BarSym}}));
              R2.finalize();
              BarMaterialized = true;
            });

        R.replace(std::move(NewMU));

        R.resolve(SymbolMap({{Foo, FooSym}}));
        R.finalize();

        FooMaterialized = true;
      });

  cantFail(V.define(MU));

  EXPECT_FALSE(FooMaterialized) << "Foo should not be materialized yet";
  EXPECT_FALSE(BarMaterialized) << "Bar should not be materialized yet";

  auto FooSymResult = cantFail(lookup({&V}, Foo));
  EXPECT_EQ(FooSymResult.getAddress(), FooSym.getAddress())
      << "Address mismatch for Foo";

  EXPECT_TRUE(FooMaterialized) << "Foo should be materialized now";
  EXPECT_FALSE(BarMaterialized) << "Bar still should not be materialized";

  auto BarSymResult = cantFail(lookup({&V}, Bar));
  EXPECT_EQ(BarSymResult.getAddress(), BarSym.getAddress())
      << "Address mismatch for Bar";
  EXPECT_TRUE(BarMaterialized) << "Bar should be materialized now";
}

TEST_F(CoreAPIsStandardTest, TestMaterializationResponsibilityDelegation) {
  auto MU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Foo, FooSym.getFlags()}, {Bar, BarSym.getFlags()}}),
      [&](MaterializationResponsibility R) {
        auto R2 = R.delegate({Bar});

        R.resolve({{Foo, FooSym}});
        R.finalize();
        R2.resolve({{Bar, BarSym}});
        R2.finalize();
      });

  cantFail(V.define(MU));

  auto Result = lookup({&V}, {Foo, Bar});

  EXPECT_TRUE(!!Result) << "Result should be a success value";
  EXPECT_EQ(Result->count(Foo), 1U) << "\"Foo\" entry missing";
  EXPECT_EQ(Result->count(Bar), 1U) << "\"Bar\" entry missing";
  EXPECT_EQ((*Result)[Foo].getAddress(), FooSym.getAddress())
      << "Address mismatch for \"Foo\"";
  EXPECT_EQ((*Result)[Bar].getAddress(), BarSym.getAddress())
      << "Address mismatch for \"Bar\"";
}

TEST_F(CoreAPIsStandardTest, TestMaterializeWeakSymbol) {
  // Confirm that once a weak definition is selected for materialization it is
  // treated as strong.
  JITSymbolFlags WeakExported = JITSymbolFlags::Exported;
  WeakExported &= JITSymbolFlags::Weak;

  std::unique_ptr<MaterializationResponsibility> FooResponsibility;
  auto MU = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Foo, FooSym.getFlags()}}),
      [&](MaterializationResponsibility R) {
        FooResponsibility =
            llvm::make_unique<MaterializationResponsibility>(std::move(R));
      });

  cantFail(V.define(MU));
  auto OnResolution = [](Expected<SymbolMap> Result) {
    cantFail(std::move(Result));
  };

  auto OnReady = [](Error Err) { cantFail(std::move(Err)); };

  ES.lookup({&V}, {Foo}, std::move(OnResolution), std::move(OnReady),
            NoDependenciesToRegister);

  auto MU2 = llvm::make_unique<SimpleMaterializationUnit>(
      SymbolFlagsMap({{Foo, JITSymbolFlags::Exported}}),
      [](MaterializationResponsibility R) {
        llvm_unreachable("This unit should never be materialized");
      });

  auto Err = V.define(MU2);
  EXPECT_TRUE(!!Err) << "Expected failure value";
  EXPECT_TRUE(Err.isA<DuplicateDefinition>())
      << "Expected a duplicate definition error";
  consumeError(std::move(Err));

  FooResponsibility->resolve(SymbolMap({{Foo, FooSym}}));
  FooResponsibility->finalize();
}

} // namespace
