//===--------- LLJIT.cpp - An ORC-based JIT for compiling LLVM IR ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/OrcError.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/Mangler.h"

namespace llvm {
namespace orc {

Expected<std::unique_ptr<LLJIT>>
LLJIT::Create(std::unique_ptr<ExecutionSession> ES,
              std::unique_ptr<TargetMachine> TM, DataLayout DL) {
  return std::unique_ptr<LLJIT>(
      new LLJIT(std::move(ES), std::move(TM), std::move(DL)));
}

Error LLJIT::defineAbsolute(StringRef Name, JITEvaluatedSymbol Sym) {
  auto InternedName = ES->getSymbolStringPool().intern(Name);
  SymbolMap Symbols({{InternedName, Sym}});
  return Main.define(absoluteSymbols(std::move(Symbols)));
}

Error LLJIT::addIRModule(VSO &V, std::unique_ptr<Module> M) {
  assert(M && "Can not add null module");

  if (auto Err = applyDataLayout(*M))
    return Err;

  auto K = ES->allocateVModule();
  return CompileLayer.add(V, K, std::move(M));
}

Expected<JITEvaluatedSymbol> LLJIT::lookupLinkerMangled(VSO &V,
                                                        StringRef Name) {
  return llvm::orc::lookup({&V}, ES->getSymbolStringPool().intern(Name));
}

LLJIT::LLJIT(std::unique_ptr<ExecutionSession> ES,
             std::unique_ptr<TargetMachine> TM, DataLayout DL)
    : ES(std::move(ES)), Main(this->ES->createVSO("main")), TM(std::move(TM)),
      DL(std::move(DL)),
      ObjLinkingLayer(*this->ES,
                      [this](VModuleKey K) { return getMemoryManager(K); }),
      CompileLayer(*this->ES, ObjLinkingLayer, SimpleCompiler(*this->TM)),
      CtorRunner(Main), DtorRunner(Main) {}

std::shared_ptr<RuntimeDyld::MemoryManager>
LLJIT::getMemoryManager(VModuleKey K) {
  return llvm::make_unique<SectionMemoryManager>();
}

std::string LLJIT::mangle(StringRef UnmangledName) {
  std::string MangledName;
  {
    raw_string_ostream MangledNameStream(MangledName);
    Mangler::getNameWithPrefix(MangledNameStream, UnmangledName, DL);
  }
  return MangledName;
}

Error LLJIT::applyDataLayout(Module &M) {
  if (M.getDataLayout().isDefault())
    M.setDataLayout(DL);

  if (M.getDataLayout() != DL)
    return make_error<StringError>(
        "Added modules have incompatible data layouts",
        inconvertibleErrorCode());

  return Error::success();
}

void LLJIT::recordCtorDtors(Module &M) {
  CtorRunner.add(getConstructors(M));
  DtorRunner.add(getDestructors(M));
}

Expected<std::unique_ptr<LLLazyJIT>>
LLLazyJIT::Create(std::unique_ptr<ExecutionSession> ES,
                  std::unique_ptr<TargetMachine> TM, DataLayout DL,
                  LLVMContext &Ctx) {
  const Triple &TT = TM->getTargetTriple();

  auto CCMgr = createLocalCompileCallbackManager(TT, *ES, 0);
  if (!CCMgr)
    return make_error<StringError>(
        std::string("No callback manager available for ") + TT.str(),
        inconvertibleErrorCode());

  auto ISMBuilder = createLocalIndirectStubsManagerBuilder(TT);
  if (!ISMBuilder)
    return make_error<StringError>(
        std::string("No indirect stubs manager builder for ") + TT.str(),
        inconvertibleErrorCode());

  return std::unique_ptr<LLLazyJIT>(
      new LLLazyJIT(std::move(ES), std::move(TM), std::move(DL), Ctx,
                    std::move(CCMgr), std::move(ISMBuilder)));
}

Error LLLazyJIT::addLazyIRModule(VSO &V, std::unique_ptr<Module> M) {
  assert(M && "Can not add null module");

  if (auto Err = applyDataLayout(*M))
    return Err;

  makeAllSymbolsExternallyAccessible(*M);

  recordCtorDtors(*M);

  auto K = ES->allocateVModule();
  return CODLayer.add(V, K, std::move(M));
}

LLLazyJIT::LLLazyJIT(
    std::unique_ptr<ExecutionSession> ES, std::unique_ptr<TargetMachine> TM,
    DataLayout DL, LLVMContext &Ctx,
    std::unique_ptr<JITCompileCallbackManager> CCMgr,
    std::function<std::unique_ptr<IndirectStubsManager>()> ISMBuilder)
    : LLJIT(std::move(ES), std::move(TM), std::move(DL)),
      CCMgr(std::move(CCMgr)), TransformLayer(*this->ES, CompileLayer),
      CODLayer(*this->ES, TransformLayer, *this->CCMgr, std::move(ISMBuilder),
               [&]() -> LLVMContext & { return Ctx; }) {}

} // End namespace orc.
} // End namespace llvm.
