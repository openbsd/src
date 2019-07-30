//===-- CommandObjectArgs.h -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_CommandObjectArgs_h_
#define liblldb_CommandObjectArgs_h_

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Interpreter/CommandObject.h"
#include "lldb/Interpreter/Options.h"

namespace lldb_private {

class CommandObjectArgs : public CommandObjectParsed {
public:
  class CommandOptions : public Options {
  public:
    CommandOptions(CommandInterpreter &interpreter);

    ~CommandOptions() override;

    Status SetOptionValue(uint32_t option_idx, llvm::StringRef option_arg,
                          ExecutionContext *execution_context) override;

    void OptionParsingStarting(ExecutionContext *execution_context) override;

    llvm::ArrayRef<OptionDefinition> GetDefinitions() override;
  };

  CommandObjectArgs(CommandInterpreter &interpreter);

  ~CommandObjectArgs() override;

  Options *GetOptions() override;

protected:
  CommandOptions m_options;

  bool DoExecute(Args &command, CommandReturnObject &result) override;
};

} // namespace lldb_private

#endif // liblldb_CommandObjectArgs_h_
