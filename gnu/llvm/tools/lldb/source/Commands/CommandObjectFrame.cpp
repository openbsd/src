//===-- CommandObjectFrame.cpp ----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// C Includes
// C++ Includes
#include <string>

// Other libraries and framework includes
// Project includes
#include "CommandObjectFrame.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/StreamFile.h"
#include "lldb/Core/Value.h"
#include "lldb/Core/ValueObject.h"
#include "lldb/Core/ValueObjectVariable.h"
#include "lldb/DataFormatters/DataVisualization.h"
#include "lldb/DataFormatters/ValueObjectPrinter.h"
#include "lldb/Host/Host.h"
#include "lldb/Host/OptionParser.h"
#include "lldb/Interpreter/Args.h"
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Interpreter/CommandReturnObject.h"
#include "lldb/Interpreter/OptionGroupFormat.h"
#include "lldb/Interpreter/OptionGroupValueObjectDisplay.h"
#include "lldb/Interpreter/OptionGroupVariable.h"
#include "lldb/Interpreter/Options.h"
#include "lldb/Symbol/ClangASTContext.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Symbol/Function.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Symbol/Type.h"
#include "lldb/Symbol/Variable.h"
#include "lldb/Symbol/VariableList.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/StopInfo.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/LLDBAssert.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/Utility/Timer.h"

using namespace lldb;
using namespace lldb_private;

#pragma mark CommandObjectFrameDiagnose

//-------------------------------------------------------------------------
// CommandObjectFrameInfo
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// CommandObjectFrameDiagnose
//-------------------------------------------------------------------------

static OptionDefinition g_frame_diag_options[] = {
    // clang-format off
  { LLDB_OPT_SET_1, false, "register", 'r', OptionParser::eRequiredArgument, nullptr, nullptr, 0, eArgTypeRegisterName,    "A register to diagnose." },
  { LLDB_OPT_SET_1, false, "address",  'a', OptionParser::eRequiredArgument, nullptr, nullptr, 0, eArgTypeAddress,         "An address to diagnose." },
  { LLDB_OPT_SET_1, false, "offset",   'o', OptionParser::eRequiredArgument, nullptr, nullptr, 0, eArgTypeOffset,          "An optional offset.  Requires --register." }
    // clang-format on
};

class CommandObjectFrameDiagnose : public CommandObjectParsed {
public:
  class CommandOptions : public Options {
  public:
    CommandOptions() : Options() { OptionParsingStarting(nullptr); }

    ~CommandOptions() override = default;

    Status SetOptionValue(uint32_t option_idx, llvm::StringRef option_arg,
                          ExecutionContext *execution_context) override {
      Status error;
      const int short_option = m_getopt_table[option_idx].val;
      switch (short_option) {
      case 'r':
        reg = ConstString(option_arg);
        break;

      case 'a': {
        address.emplace();
        if (option_arg.getAsInteger(0, *address)) {
          address.reset();
          error.SetErrorStringWithFormat("invalid address argument '%s'",
                                         option_arg.str().c_str());
        }
      } break;

      case 'o': {
        offset.emplace();
        if (option_arg.getAsInteger(0, *offset)) {
          offset.reset();
          error.SetErrorStringWithFormat("invalid offset argument '%s'",
                                         option_arg.str().c_str());
        }
      } break;

      default:
        error.SetErrorStringWithFormat("invalid short option character '%c'",
                                       short_option);
        break;
      }

      return error;
    }

    void OptionParsingStarting(ExecutionContext *execution_context) override {
      address.reset();
      reg.reset();
      offset.reset();
    }

    llvm::ArrayRef<OptionDefinition> GetDefinitions() override {
      return llvm::makeArrayRef(g_frame_diag_options);
    }

    // Options.
    llvm::Optional<lldb::addr_t> address;
    llvm::Optional<ConstString> reg;
    llvm::Optional<int64_t> offset;
  };

  CommandObjectFrameDiagnose(CommandInterpreter &interpreter)
      : CommandObjectParsed(interpreter, "frame diagnose",
                            "Try to determine what path path the current stop "
                            "location used to get to a register or address",
                            nullptr,
                            eCommandRequiresThread | eCommandTryTargetAPILock |
                                eCommandProcessMustBeLaunched |
                                eCommandProcessMustBePaused),
        m_options() {
    CommandArgumentEntry arg;
    CommandArgumentData index_arg;

    // Define the first (and only) variant of this arg.
    index_arg.arg_type = eArgTypeFrameIndex;
    index_arg.arg_repetition = eArgRepeatOptional;

    // There is only one variant this argument could be; put it into the
    // argument entry.
    arg.push_back(index_arg);

    // Push the data for the first argument into the m_arguments vector.
    m_arguments.push_back(arg);
  }

  ~CommandObjectFrameDiagnose() override = default;

  Options *GetOptions() override { return &m_options; }

protected:
  bool DoExecute(Args &command, CommandReturnObject &result) override {
    Thread *thread = m_exe_ctx.GetThreadPtr();
    StackFrameSP frame_sp = thread->GetSelectedFrame();

    ValueObjectSP valobj_sp;

    if (m_options.address.hasValue()) {
      if (m_options.reg.hasValue() || m_options.offset.hasValue()) {
        result.AppendError(
            "`frame diagnose --address` is incompatible with other arguments.");
        result.SetStatus(eReturnStatusFailed);
        return false;
      }
      valobj_sp = frame_sp->GuessValueForAddress(m_options.address.getValue());
    } else if (m_options.reg.hasValue()) {
      valobj_sp = frame_sp->GuessValueForRegisterAndOffset(
          m_options.reg.getValue(), m_options.offset.getValueOr(0));
    } else {
      StopInfoSP stop_info_sp = thread->GetStopInfo();
      if (!stop_info_sp) {
        result.AppendError("No arguments provided, and no stop info.");
        result.SetStatus(eReturnStatusFailed);
        return false;
      }

      valobj_sp = StopInfo::GetCrashingDereference(stop_info_sp);
    }

    if (!valobj_sp) {
      result.AppendError("No diagnosis available.");
      result.SetStatus(eReturnStatusFailed);
      return false;
    }


    DumpValueObjectOptions::DeclPrintingHelper helper = [&valobj_sp](
        ConstString type, ConstString var, const DumpValueObjectOptions &opts,
        Stream &stream) -> bool {
      const ValueObject::GetExpressionPathFormat format = ValueObject::
          GetExpressionPathFormat::eGetExpressionPathFormatHonorPointers;
      const bool qualify_cxx_base_classes = false;
      valobj_sp->GetExpressionPath(stream, qualify_cxx_base_classes, format);
      stream.PutCString(" =");
      return true;
    };

    DumpValueObjectOptions options;
    options.SetDeclPrintingHelper(helper);
    ValueObjectPrinter printer(valobj_sp.get(), &result.GetOutputStream(),
                               options);
    printer.PrintValueObject();

    return true;
  }

protected:
  CommandOptions m_options;
};

#pragma mark CommandObjectFrameInfo

//-------------------------------------------------------------------------
// CommandObjectFrameInfo
//-------------------------------------------------------------------------

class CommandObjectFrameInfo : public CommandObjectParsed {
public:
  CommandObjectFrameInfo(CommandInterpreter &interpreter)
      : CommandObjectParsed(
            interpreter, "frame info", "List information about the current "
                                       "stack frame in the current thread.",
            "frame info",
            eCommandRequiresFrame | eCommandTryTargetAPILock |
                eCommandProcessMustBeLaunched | eCommandProcessMustBePaused) {}

  ~CommandObjectFrameInfo() override = default;

protected:
  bool DoExecute(Args &command, CommandReturnObject &result) override {
    m_exe_ctx.GetFrameRef().DumpUsingSettingsFormat(&result.GetOutputStream());
    result.SetStatus(eReturnStatusSuccessFinishResult);
    return result.Succeeded();
  }
};

#pragma mark CommandObjectFrameSelect

//-------------------------------------------------------------------------
// CommandObjectFrameSelect
//-------------------------------------------------------------------------

static OptionDefinition g_frame_select_options[] = {
    // clang-format off
  { LLDB_OPT_SET_1, false, "relative", 'r', OptionParser::eRequiredArgument, nullptr, nullptr, 0, eArgTypeOffset, "A relative frame index offset from the current frame index." },
    // clang-format on
};

class CommandObjectFrameSelect : public CommandObjectParsed {
public:
  class CommandOptions : public Options {
  public:
    CommandOptions() : Options() { OptionParsingStarting(nullptr); }

    ~CommandOptions() override = default;

    Status SetOptionValue(uint32_t option_idx, llvm::StringRef option_arg,
                          ExecutionContext *execution_context) override {
      Status error;
      const int short_option = m_getopt_table[option_idx].val;
      switch (short_option) {
      case 'r':
        if (option_arg.getAsInteger(0, relative_frame_offset)) {
          relative_frame_offset = INT32_MIN;
          error.SetErrorStringWithFormat("invalid frame offset argument '%s'",
                                         option_arg.str().c_str());
        }
        break;

      default:
        error.SetErrorStringWithFormat("invalid short option character '%c'",
                                       short_option);
        break;
      }

      return error;
    }

    void OptionParsingStarting(ExecutionContext *execution_context) override {
      relative_frame_offset = INT32_MIN;
    }

    llvm::ArrayRef<OptionDefinition> GetDefinitions() override {
      return llvm::makeArrayRef(g_frame_select_options);
    }

    int32_t relative_frame_offset;
  };

  CommandObjectFrameSelect(CommandInterpreter &interpreter)
      : CommandObjectParsed(
            interpreter, "frame select", "Select the current stack frame by "
                                         "index from within the current thread "
                                         "(see 'thread backtrace'.)",
            nullptr,
            eCommandRequiresThread | eCommandTryTargetAPILock |
                eCommandProcessMustBeLaunched | eCommandProcessMustBePaused),
        m_options() {
    CommandArgumentEntry arg;
    CommandArgumentData index_arg;

    // Define the first (and only) variant of this arg.
    index_arg.arg_type = eArgTypeFrameIndex;
    index_arg.arg_repetition = eArgRepeatOptional;

    // There is only one variant this argument could be; put it into the
    // argument entry.
    arg.push_back(index_arg);

    // Push the data for the first argument into the m_arguments vector.
    m_arguments.push_back(arg);
  }

  ~CommandObjectFrameSelect() override = default;

  Options *GetOptions() override { return &m_options; }

protected:
  bool DoExecute(Args &command, CommandReturnObject &result) override {
    // No need to check "thread" for validity as eCommandRequiresThread ensures
    // it is valid
    Thread *thread = m_exe_ctx.GetThreadPtr();

    uint32_t frame_idx = UINT32_MAX;
    if (m_options.relative_frame_offset != INT32_MIN) {
      // The one and only argument is a signed relative frame index
      frame_idx = thread->GetSelectedFrameIndex();
      if (frame_idx == UINT32_MAX)
        frame_idx = 0;

      if (m_options.relative_frame_offset < 0) {
        if (static_cast<int32_t>(frame_idx) >= -m_options.relative_frame_offset)
          frame_idx += m_options.relative_frame_offset;
        else {
          if (frame_idx == 0) {
            // If you are already at the bottom of the stack, then just warn and
            // don't reset the frame.
            result.AppendError("Already at the bottom of the stack.");
            result.SetStatus(eReturnStatusFailed);
            return false;
          } else
            frame_idx = 0;
        }
      } else if (m_options.relative_frame_offset > 0) {
        // I don't want "up 20" where "20" takes you past the top of the stack
        // to produce
        // an error, but rather to just go to the top.  So I have to count the
        // stack here...
        const uint32_t num_frames = thread->GetStackFrameCount();
        if (static_cast<int32_t>(num_frames - frame_idx) >
            m_options.relative_frame_offset)
          frame_idx += m_options.relative_frame_offset;
        else {
          if (frame_idx == num_frames - 1) {
            // If we are already at the top of the stack, just warn and don't
            // reset the frame.
            result.AppendError("Already at the top of the stack.");
            result.SetStatus(eReturnStatusFailed);
            return false;
          } else
            frame_idx = num_frames - 1;
        }
      }
    } else {
      if (command.GetArgumentCount() > 1) {
        result.AppendErrorWithFormat(
            "too many arguments; expected frame-index, saw '%s'.\n",
            command[0].c_str());
        m_options.GenerateOptionUsage(
            result.GetErrorStream(), this,
            GetCommandInterpreter().GetDebugger().GetTerminalWidth());
        return false;
      }

      if (command.GetArgumentCount() == 1) {
        if (command[0].ref.getAsInteger(0, frame_idx)) {
          result.AppendErrorWithFormat("invalid frame index argument '%s'.",
                                       command[0].c_str());
          result.SetStatus(eReturnStatusFailed);
          return false;
        }
      } else if (command.GetArgumentCount() == 0) {
        frame_idx = thread->GetSelectedFrameIndex();
        if (frame_idx == UINT32_MAX) {
          frame_idx = 0;
        }
      }
    }

    bool success = thread->SetSelectedFrameByIndexNoisily(
        frame_idx, result.GetOutputStream());
    if (success) {
      m_exe_ctx.SetFrameSP(thread->GetSelectedFrame());
      result.SetStatus(eReturnStatusSuccessFinishResult);
    } else {
      result.AppendErrorWithFormat("Frame index (%u) out of range.\n",
                                   frame_idx);
      result.SetStatus(eReturnStatusFailed);
    }

    return result.Succeeded();
  }

protected:
  CommandOptions m_options;
};

#pragma mark CommandObjectFrameVariable
//----------------------------------------------------------------------
// List images with associated information
//----------------------------------------------------------------------
class CommandObjectFrameVariable : public CommandObjectParsed {
public:
  CommandObjectFrameVariable(CommandInterpreter &interpreter)
      : CommandObjectParsed(
            interpreter, "frame variable",
            "Show variables for the current stack frame. Defaults to all "
            "arguments and local variables in scope. Names of argument, "
            "local, file static and file global variables can be specified. "
            "Children of aggregate variables can be specified such as "
            "'var->child.x'.",
            nullptr, eCommandRequiresFrame | eCommandTryTargetAPILock |
                         eCommandProcessMustBeLaunched |
                         eCommandProcessMustBePaused | eCommandRequiresProcess),
        m_option_group(),
        m_option_variable(
            true), // Include the frame specific options by passing "true"
        m_option_format(eFormatDefault),
        m_varobj_options() {
    CommandArgumentEntry arg;
    CommandArgumentData var_name_arg;

    // Define the first (and only) variant of this arg.
    var_name_arg.arg_type = eArgTypeVarName;
    var_name_arg.arg_repetition = eArgRepeatStar;

    // There is only one variant this argument could be; put it into the
    // argument entry.
    arg.push_back(var_name_arg);

    // Push the data for the first argument into the m_arguments vector.
    m_arguments.push_back(arg);

    m_option_group.Append(&m_option_variable, LLDB_OPT_SET_ALL, LLDB_OPT_SET_1);
    m_option_group.Append(&m_option_format,
                          OptionGroupFormat::OPTION_GROUP_FORMAT |
                              OptionGroupFormat::OPTION_GROUP_GDB_FMT,
                          LLDB_OPT_SET_1);
    m_option_group.Append(&m_varobj_options, LLDB_OPT_SET_ALL, LLDB_OPT_SET_1);
    m_option_group.Finalize();
  }

  ~CommandObjectFrameVariable() override = default;

  Options *GetOptions() override { return &m_option_group; }

  int HandleArgumentCompletion(Args &input, int &cursor_index,
                               int &cursor_char_position,
                               OptionElementVector &opt_element_vector,
                               int match_start_point, int max_return_elements,
                               bool &word_complete,
                               StringList &matches) override {
    // Arguments are the standard source file completer.
    auto completion_str = input[cursor_index].ref;
    completion_str = completion_str.take_front(cursor_char_position);

    CommandCompletions::InvokeCommonCompletionCallbacks(
        GetCommandInterpreter(), CommandCompletions::eVariablePathCompletion,
        completion_str, match_start_point, max_return_elements, nullptr,
        word_complete, matches);
    return matches.GetSize();
  }

protected:
  llvm::StringRef GetScopeString(VariableSP var_sp) {
    if (!var_sp)
      return llvm::StringRef::withNullAsEmpty(nullptr);

    switch (var_sp->GetScope()) {
    case eValueTypeVariableGlobal:
      return "GLOBAL: ";
    case eValueTypeVariableStatic:
      return "STATIC: ";
    case eValueTypeVariableArgument:
      return "ARG: ";
    case eValueTypeVariableLocal:
      return "LOCAL: ";
    case eValueTypeVariableThreadLocal:
      return "THREAD: ";
    default:
      break;
    }

    return llvm::StringRef::withNullAsEmpty(nullptr);
  }

  bool DoExecute(Args &command, CommandReturnObject &result) override {
    // No need to check "frame" for validity as eCommandRequiresFrame ensures it
    // is valid
    StackFrame *frame = m_exe_ctx.GetFramePtr();

    Stream &s = result.GetOutputStream();

    // Be careful about the stack frame, if any summary formatter runs code, it
    // might clear the StackFrameList
    // for the thread.  So hold onto a shared pointer to the frame so it stays
    // alive.

    VariableList *variable_list =
        frame->GetVariableList(m_option_variable.show_globals);

    VariableSP var_sp;
    ValueObjectSP valobj_sp;

    TypeSummaryImplSP summary_format_sp;
    if (!m_option_variable.summary.IsCurrentValueEmpty())
      DataVisualization::NamedSummaryFormats::GetSummaryFormat(
          ConstString(m_option_variable.summary.GetCurrentValue()),
          summary_format_sp);
    else if (!m_option_variable.summary_string.IsCurrentValueEmpty())
      summary_format_sp.reset(new StringSummaryFormat(
          TypeSummaryImpl::Flags(),
          m_option_variable.summary_string.GetCurrentValue()));

    DumpValueObjectOptions options(m_varobj_options.GetAsDumpOptions(
        eLanguageRuntimeDescriptionDisplayVerbosityFull, eFormatDefault,
        summary_format_sp));

    const SymbolContext &sym_ctx =
        frame->GetSymbolContext(eSymbolContextFunction);
    if (sym_ctx.function && sym_ctx.function->IsTopLevelFunction())
      m_option_variable.show_globals = true;

    if (variable_list) {
      const Format format = m_option_format.GetFormat();
      options.SetFormat(format);

      if (!command.empty()) {
        VariableList regex_var_list;

        // If we have any args to the variable command, we will make
        // variable objects from them...
        for (auto &entry : command) {
          if (m_option_variable.use_regex) {
            const size_t regex_start_index = regex_var_list.GetSize();
            llvm::StringRef name_str = entry.ref;
            RegularExpression regex(name_str);
            if (regex.Compile(name_str)) {
              size_t num_matches = 0;
              const size_t num_new_regex_vars =
                  variable_list->AppendVariablesIfUnique(regex, regex_var_list,
                                                         num_matches);
              if (num_new_regex_vars > 0) {
                for (size_t regex_idx = regex_start_index,
                            end_index = regex_var_list.GetSize();
                     regex_idx < end_index; ++regex_idx) {
                  var_sp = regex_var_list.GetVariableAtIndex(regex_idx);
                  if (var_sp) {
                    valobj_sp = frame->GetValueObjectForFrameVariable(
                        var_sp, m_varobj_options.use_dynamic);
                    if (valobj_sp) {
                      std::string scope_string;
                      if (m_option_variable.show_scope)
                        scope_string = GetScopeString(var_sp).str();

                      if (!scope_string.empty())
                        s.PutCString(scope_string);

                      if (m_option_variable.show_decl &&
                          var_sp->GetDeclaration().GetFile()) {
                        bool show_fullpaths = false;
                        bool show_module = true;
                        if (var_sp->DumpDeclaration(&s, show_fullpaths,
                                                    show_module))
                          s.PutCString(": ");
                      }
                      valobj_sp->Dump(result.GetOutputStream(), options);
                    }
                  }
                }
              } else if (num_matches == 0) {
                result.GetErrorStream().Printf("error: no variables matched "
                                               "the regular expression '%s'.\n",
                                               entry.c_str());
              }
            } else {
              char regex_error[1024];
              if (regex.GetErrorAsCString(regex_error, sizeof(regex_error)))
                result.GetErrorStream().Printf("error: %s\n", regex_error);
              else
                result.GetErrorStream().Printf(
                    "error: unknown regex error when compiling '%s'\n",
                    entry.c_str());
            }
          } else // No regex, either exact variable names or variable
                 // expressions.
          {
            Status error;
            uint32_t expr_path_options =
                StackFrame::eExpressionPathOptionCheckPtrVsMember |
                StackFrame::eExpressionPathOptionsAllowDirectIVarAccess |
                StackFrame::eExpressionPathOptionsInspectAnonymousUnions;
            lldb::VariableSP var_sp;
            valobj_sp = frame->GetValueForVariableExpressionPath(
                entry.ref, m_varobj_options.use_dynamic, expr_path_options,
                var_sp, error);
            if (valobj_sp) {
              std::string scope_string;
              if (m_option_variable.show_scope)
                scope_string = GetScopeString(var_sp).str();

              if (!scope_string.empty())
                s.PutCString(scope_string);

              //                            if (format != eFormatDefault)
              //                                valobj_sp->SetFormat (format);
              if (m_option_variable.show_decl && var_sp &&
                  var_sp->GetDeclaration().GetFile()) {
                var_sp->GetDeclaration().DumpStopContext(&s, false);
                s.PutCString(": ");
              }

              options.SetFormat(format);
              options.SetVariableFormatDisplayLanguage(
                  valobj_sp->GetPreferredDisplayLanguage());

              Stream &output_stream = result.GetOutputStream();
              options.SetRootValueObjectName(
                  valobj_sp->GetParent() ? entry.c_str() : nullptr);
              valobj_sp->Dump(output_stream, options);
            } else {
              const char *error_cstr = error.AsCString(nullptr);
              if (error_cstr)
                result.GetErrorStream().Printf("error: %s\n", error_cstr);
              else
                result.GetErrorStream().Printf("error: unable to find any "
                                               "variable expression path that "
                                               "matches '%s'.\n",
                                               entry.c_str());
            }
          }
        }
      } else // No command arg specified.  Use variable_list, instead.
      {
        const size_t num_variables = variable_list->GetSize();
        if (num_variables > 0) {
          for (size_t i = 0; i < num_variables; i++) {
            var_sp = variable_list->GetVariableAtIndex(i);
            switch (var_sp->GetScope()) {
            case eValueTypeVariableGlobal:
              if (!m_option_variable.show_globals)
                continue;
              break;
            case eValueTypeVariableStatic:
              if (!m_option_variable.show_globals)
                continue;
              break;
            case eValueTypeVariableArgument:
              if (!m_option_variable.show_args)
                continue;
              break;
            case eValueTypeVariableLocal:
              if (!m_option_variable.show_locals)
                continue;
              break;
            default:
              continue;
              break;
            }
            std::string scope_string;
            if (m_option_variable.show_scope)
              scope_string = GetScopeString(var_sp).str();

            // Use the variable object code to make sure we are
            // using the same APIs as the public API will be
            // using...
            valobj_sp = frame->GetValueObjectForFrameVariable(
                var_sp, m_varobj_options.use_dynamic);
            if (valobj_sp) {
              // When dumping all variables, don't print any variables
              // that are not in scope to avoid extra unneeded output
              if (valobj_sp->IsInScope()) {
                if (!valobj_sp->GetTargetSP()
                         ->GetDisplayRuntimeSupportValues() &&
                    valobj_sp->IsRuntimeSupportValue())
                  continue;

                if (!scope_string.empty())
                  s.PutCString(scope_string);

                if (m_option_variable.show_decl &&
                    var_sp->GetDeclaration().GetFile()) {
                  var_sp->GetDeclaration().DumpStopContext(&s, false);
                  s.PutCString(": ");
                }

                options.SetFormat(format);
                options.SetVariableFormatDisplayLanguage(
                    valobj_sp->GetPreferredDisplayLanguage());
                options.SetRootValueObjectName(
                    var_sp ? var_sp->GetName().AsCString() : nullptr);
                valobj_sp->Dump(result.GetOutputStream(), options);
              }
            }
          }
        }
      }
      result.SetStatus(eReturnStatusSuccessFinishResult);
    }

    if (m_interpreter.TruncationWarningNecessary()) {
      result.GetOutputStream().Printf(m_interpreter.TruncationWarningText(),
                                      m_cmd_name.c_str());
      m_interpreter.TruncationWarningGiven();
    }

    return result.Succeeded();
  }

protected:
  OptionGroupOptions m_option_group;
  OptionGroupVariable m_option_variable;
  OptionGroupFormat m_option_format;
  OptionGroupValueObjectDisplay m_varobj_options;
};

#pragma mark CommandObjectMultiwordFrame

//-------------------------------------------------------------------------
// CommandObjectMultiwordFrame
//-------------------------------------------------------------------------

CommandObjectMultiwordFrame::CommandObjectMultiwordFrame(
    CommandInterpreter &interpreter)
    : CommandObjectMultiword(interpreter, "frame", "Commands for selecting and "
                                                   "examing the current "
                                                   "thread's stack frames.",
                             "frame <subcommand> [<subcommand-options>]") {
  LoadSubCommand("diagnose",
                 CommandObjectSP(new CommandObjectFrameDiagnose(interpreter)));
  LoadSubCommand("info",
                 CommandObjectSP(new CommandObjectFrameInfo(interpreter)));
  LoadSubCommand("select",
                 CommandObjectSP(new CommandObjectFrameSelect(interpreter)));
  LoadSubCommand("variable",
                 CommandObjectSP(new CommandObjectFrameVariable(interpreter)));
}

CommandObjectMultiwordFrame::~CommandObjectMultiwordFrame() = default;
