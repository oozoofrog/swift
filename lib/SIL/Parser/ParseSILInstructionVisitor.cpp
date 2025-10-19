//===--- ParseSILInstructionVisitor.cpp - SIL Instruction Visitor ---------===//
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
// This file implements the SILInstructionParserVisitor class, which uses
// a visitor pattern to dispatch SIL instruction parsing.
//
// Related GitHub Issue: swift#42962 (SR-340)
//
//===----------------------------------------------------------------------===//

#include "ParseSILInstructionVisitor.h"
#include "SILParser.h"
#include "swift/SIL/SILBuilder.h"

using namespace swift;

//===----------------------------------------------------------------------===//
// Dispatch Implementation
//===----------------------------------------------------------------------===//

std::optional<bool>
SILInstructionParserVisitor::dispatch(SILInstructionKind Opcode) {
  switch (Opcode) {
#define PARSE_SIL_VISITOR_ENTRY(ID)                                            \
  case SILInstructionKind::ID:                                                 \
    return visit##ID();
#include "SILInstructionParser.def"
  default:
    // Instruction not yet migrated to visitor pattern
    // Return std::nullopt to signal fallback to legacy switch
    return std::nullopt;
  }
}

//===----------------------------------------------------------------------===//
// Visit Method Implementations
//===----------------------------------------------------------------------===//

// Phase 0: Stub implementation for AllocBoxInst
// This will be replaced with actual parsing logic migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAllocBoxInst() {
  // TODO: Migrate actual implementation from ParseSIL.cpp
  // For now, return false (indicating parse failure) to trigger fallback
  return false;
}
