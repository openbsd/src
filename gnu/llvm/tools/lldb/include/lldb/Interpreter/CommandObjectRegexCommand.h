//===-- CommandObjectRegexCommand.h -----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_CommandObjectRegexCommand_h_
#define liblldb_CommandObjectRegexCommand_h_

// C Includes
// C++ Includes
#include <list>

// Other libraries and framework includes
// Project includes
#include "lldb/Interpreter/CommandObject.h"
#include "lldb/Utility/RegularExpression.h"

namespace lldb_private {

//-------------------------------------------------------------------------
// CommandObjectRegexCommand
//-------------------------------------------------------------------------

class CommandObjectRegexCommand : public CommandObjectRaw {
public:
  CommandObjectRegexCommand(CommandInterpreter &interpreter, llvm::StringRef name,
    llvm::StringRef help, llvm::StringRef syntax,
                            uint32_t max_matches, uint32_t completion_type_mask,
                            bool is_removable);

  ~CommandObjectRegexCommand() override;

  bool IsRemovable() const override { return m_is_removable; }

  bool AddRegexCommand(const char *re_cstr, const char *command_cstr);

  bool HasRegexEntries() const { return !m_entries.empty(); }

  int HandleCompletion(Args &input, int &cursor_index,
                       int &cursor_char_position, int match_start_point,
                       int max_return_elements, bool &word_complete,
                       StringList &matches) override;

protected:
  bool DoExecute(const char *command, CommandReturnObject &result) override;

  struct Entry {
    RegularExpression regex;
    std::string command;
  };

  typedef std::list<Entry> EntryCollection;
  const uint32_t m_max_matches;
  const uint32_t m_completion_type_mask;
  EntryCollection m_entries;
  bool m_is_removable;

private:
  DISALLOW_COPY_AND_ASSIGN(CommandObjectRegexCommand);
};

} // namespace lldb_private

#endif // liblldb_CommandObjectRegexCommand_h_
