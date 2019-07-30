//===-- DisassemblerLLVMC.h -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_DisassemblerLLVMC_h_
#define liblldb_DisassemblerLLVMC_h_

// C Includes
// C++ Includes
#include <memory>
#include <mutex>
#include <string>

// Other libraries and framework includes
#include "llvm-c/Disassembler.h"

// Project includes
#include "lldb/Core/Address.h"
#include "lldb/Core/Disassembler.h"
#include "lldb/Core/PluginManager.h"

// Opaque references to C++ Objects in LLVM's MC.
namespace llvm {
class MCContext;
class MCInst;
class MCInstrInfo;
class MCRegisterInfo;
class MCDisassembler;
class MCInstPrinter;
class MCAsmInfo;
class MCSubtargetInfo;
} // namespace llvm

class InstructionLLVMC;

class DisassemblerLLVMC : public lldb_private::Disassembler {
  // Since we need to make two actual MC Disassemblers for ARM (ARM & THUMB),
  // and there's a bit of goo to set up and own
  // in the MC disassembler world, I added this class to manage the actual
  // disassemblers.
  class LLVMCDisassembler {
  public:
    LLVMCDisassembler(const char *triple, const char *cpu,
                      const char *features_str, unsigned flavor,
                      DisassemblerLLVMC &owner);

    ~LLVMCDisassembler();

    uint64_t GetMCInst(const uint8_t *opcode_data, size_t opcode_data_len,
                       lldb::addr_t pc, llvm::MCInst &mc_inst);
    void PrintMCInst(llvm::MCInst &mc_inst, std::string &inst_string,
                     std::string &comments_string);
    void SetStyle(bool use_hex_immed, HexImmediateStyle hex_style);
    bool CanBranch(llvm::MCInst &mc_inst);
    bool HasDelaySlot(llvm::MCInst &mc_inst);
    bool IsCall(llvm::MCInst &mc_inst);
    bool IsValid() { return m_is_valid; }

  private:
    bool m_is_valid;
    std::unique_ptr<llvm::MCContext> m_context_ap;
    std::unique_ptr<llvm::MCAsmInfo> m_asm_info_ap;
    std::unique_ptr<llvm::MCSubtargetInfo> m_subtarget_info_ap;
    std::unique_ptr<llvm::MCInstrInfo> m_instr_info_ap;
    std::unique_ptr<llvm::MCRegisterInfo> m_reg_info_ap;
    std::unique_ptr<llvm::MCInstPrinter> m_instr_printer_ap;
    std::unique_ptr<llvm::MCDisassembler> m_disasm_ap;
  };

public:
  DisassemblerLLVMC(const lldb_private::ArchSpec &arch,
                    const char *flavor /* = NULL */);

  ~DisassemblerLLVMC() override;

  //------------------------------------------------------------------
  // Static Functions
  //------------------------------------------------------------------
  static void Initialize();

  static void Terminate();

  static lldb_private::ConstString GetPluginNameStatic();

  static lldb_private::Disassembler *
  CreateInstance(const lldb_private::ArchSpec &arch, const char *flavor);

  size_t DecodeInstructions(const lldb_private::Address &base_addr,
                            const lldb_private::DataExtractor &data,
                            lldb::offset_t data_offset, size_t num_instructions,
                            bool append, bool data_from_file) override;

  //------------------------------------------------------------------
  // PluginInterface protocol
  //------------------------------------------------------------------
  lldb_private::ConstString GetPluginName() override;

  uint32_t GetPluginVersion() override;

protected:
  friend class InstructionLLVMC;

  bool FlavorValidForArchSpec(const lldb_private::ArchSpec &arch,
                              const char *flavor) override;

  bool IsValid() {
    return (m_disasm_ap.get() != NULL && m_disasm_ap->IsValid());
  }

  int OpInfo(uint64_t PC, uint64_t Offset, uint64_t Size, int TagType,
             void *TagBug);

  const char *SymbolLookup(uint64_t ReferenceValue, uint64_t *ReferenceType,
                           uint64_t ReferencePC, const char **ReferenceName);

  static int OpInfoCallback(void *DisInfo, uint64_t PC, uint64_t Offset,
                            uint64_t Size, int TagType, void *TagBug);

  static const char *SymbolLookupCallback(void *DisInfo,
                                          uint64_t ReferenceValue,
                                          uint64_t *ReferenceType,
                                          uint64_t ReferencePC,
                                          const char **ReferenceName);

  void Lock(InstructionLLVMC *inst,
            const lldb_private::ExecutionContext *exe_ctx) {
    m_mutex.lock();
    m_inst = inst;
    m_exe_ctx = exe_ctx;
  }

  void Unlock() {
    m_inst = NULL;
    m_exe_ctx = NULL;
    m_mutex.unlock();
  }

  const lldb_private::ExecutionContext *m_exe_ctx;
  InstructionLLVMC *m_inst;
  std::mutex m_mutex;
  bool m_data_from_file;

  std::unique_ptr<LLVMCDisassembler> m_disasm_ap;
  std::unique_ptr<LLVMCDisassembler> m_alternate_disasm_ap;
};

#endif // liblldb_DisassemblerLLVM_h_
