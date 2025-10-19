//===--- ParseSILInstructionVisitor.h - SIL Instruction Visitor -*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2025 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file defines the SILInstructionParserVisitor class, which implements
// a visitor pattern for parsing SIL instructions. This is part of the
// refactoring effort to replace the large switch statement in
// parseSpecificSILInstruction with a modular visitor-based design.
//
// Related GitHub Issue: swift#42962 (SR-340)
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_SIL_PARSER_PARSESILINSTRUCTION_VISITOR_H
#define SWIFT_SIL_PARSER_PARSESILINSTRUCTION_VISITOR_H

#include "swift/SIL/SILInstruction.h"
#include "swift/SIL/SILLocation.h"
#include "llvm/ADT/StringRef.h"
#include <optional>

namespace swift {

class SILBuilder;
class SILParser;

/// Visitor class for parsing SIL instructions.
///
/// This class uses a visitor pattern to dispatch instruction parsing to
/// individual visit methods, one per instruction kind. This replaces the
/// monolithic switch statement in parseSpecificSILInstruction.
///
/// The visitor maintains references to the parser context (SILParser),
/// the builder for constructing instructions (SILBuilder), and the
/// instruction opcode information.
class SILInstructionParserVisitor {
  /// Reference to the SIL parser context
  SILParser &P;

  /// Builder for constructing SIL instructions
  SILBuilder &B;

  /// Source location of the instruction opcode
  SourceLoc OpcodeLoc;

  /// String name of the instruction opcode
  StringRef OpcodeName;

  /// Output parameter for the parsed instruction result
  SILInstruction *&ResultVal;

public:
  /// Constructor
  ///
  /// \param P The SIL parser context
  /// \param B The SIL builder for constructing instructions
  /// \param OpcodeLoc Source location of the instruction opcode
  /// \param OpcodeName String name of the instruction opcode
  /// \param ResultVal Output parameter for the parsed instruction
  SILInstructionParserVisitor(SILParser &P, SILBuilder &B,
                               SourceLoc OpcodeLoc, StringRef OpcodeName,
                               SILInstruction *&ResultVal)
      : P(P), B(B), OpcodeLoc(OpcodeLoc), OpcodeName(OpcodeName),
        ResultVal(ResultVal) {}

  /// Dispatch to the appropriate visit method based on instruction kind
  ///
  /// \param Opcode The kind of instruction to parse
  /// \return std::optional<bool> containing the parse result if handled,
  ///         or std::nullopt if the instruction is not yet migrated to the
  ///         visitor pattern (falls back to legacy switch)
  std::optional<bool> dispatch(SILInstructionKind Opcode);

  /// Visit methods for individual instruction kinds
  /// These are generated from SILInstructionParser.def
#define PARSE_SIL_VISITOR_ENTRY(ID) bool visit##ID();
#include "SILInstructionParser.def"
};

} // end namespace swift

#endif // SWIFT_SIL_PARSER_PARSESILINSTRUCTION_VISITOR_H
