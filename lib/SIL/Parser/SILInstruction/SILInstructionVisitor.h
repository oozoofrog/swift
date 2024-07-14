//===--- SILInstructionVisitor.h - SIL Instruction Visitor ------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2024 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// Visitor pattern for SIL instructions parsing.
//
//===----------------------------------------------------------------------===//

#ifndef SILINSTRUCTIONVISITOR_H
#define SILINSTRUCTIONVISITOR_H

#include "SILBuilder.h"
#include "SILLocation.h"
#include "SILInstruction.h"
#include "SILParser.h"

/// Enum to represent the result of visiting a SIL instruction
enum class VisitResult {
  Success,    ///< Instruction was successfully parsed and created
  Failure,    ///< Parsing or creation failed
  Unhandled   ///< Visitor did not handle this instruction
};

class SILInstructionVisitor {
public:
  virtual ~SILInstructionVisitor() = default;
  /// Visit method to be implemented by concrete visitors.
  virtual VisitResult visit(SILBuilder &B, SILLocation &InstLoc, SILParser &P, SILInstruction *&ResultVal) = 0;
};

class SILInstructionVisitorFactory {
public:
    static std::unique_ptr<SILInstructionVisitor> createVisitor(SILInstructionKind Opcode) {
        switch (Opcode) {
            case SILInstructionKind::AllocBoxInst:
                return std::make_unique<AllocBoxInstructionVisitor>();
            default:
                return nullptr;
        }
    }
};

/// Visitor class for parsing AllocBox instructions in SIL.
class AllocBoxInstructionVisitor : public SILInstructionVisitor {
public:
  /// Parse and create an AllocBox instruction.
  ///
  /// This method parses the AllocBox instruction from the SIL input,
  /// including its attributes and operands, and creates the corresponding
  /// SIL instruction using the provided SILBuilder.
  ///
  /// @param B The SILBuilder used to create the instruction.
  /// @param InstLoc The source location of the instruction.
  /// @param P The SILParser instance used for parsing.
  /// @param ResultVal [out] The resulting SILInstruction.
  VisitResult visit(SILBuilder &B, SILLocation &InstLoc, SILParser &P, SILInstruction *&ResultVal) override;
};

#endif // SILINSTRUCTIONVISITOR_H
