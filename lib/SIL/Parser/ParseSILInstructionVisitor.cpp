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
#include "swift/SIL/SILDebugVariable.h"
#include "swift/SIL/SILLocation.h"
#include "swift/SIL/ValueUtils.h"
#include "swift/Parse/Lexer.h"

using namespace swift;

//===----------------------------------------------------------------------===//
// Helper Functions (from ParseSIL.cpp)
//===----------------------------------------------------------------------===//

// Forward declaration of parseSILOptional from ParseSIL.cpp
// These are static functions in ParseSIL.cpp that we need to access
namespace {

// Helper to parse optional attributes like [dynamic_lifetime]
static bool parseSILOptional(StringRef &attrName, SourceLoc &attrLoc,
                             SILParser &SP) {
  if (!SP.P.consumeIf(tok::l_square))
    return false;

  Identifier parsedNameId;
  if (SP.parseSILIdentifier(parsedNameId, attrLoc,
                            diag::expected_in_attribute_list))
    return true;
  attrName = parsedNameId.str();

  if (SP.P.parseToken(tok::r_square, diag::expected_in_attribute_list))
    return true;

  return true;
}

// Helper to parse expected optional boolean attributes like [take] or [init]
static bool parseSILOptional(bool &Result, SILParser &SP, StringRef Expected) {
  StringRef Optional;
  SourceLoc Loc;
  if (parseSILOptional(Optional, Loc, SP)) {
    if (Optional != Expected) {
      SP.P.diagnose(Loc, diag::sil_invalid_attribute_for_expected, Optional,
                    Expected);
      return true;
    }
    Result = true;
  }
  return false;
}

} // end anonymous namespace

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

// Phase-based implementation files
// Each phase is split into a separate .inc file for maintainability
#include "ParseSILInstructionVisitorImpl/Phase1_Alloc.inc"
#include "ParseSILInstructionVisitorImpl/Phase2_Apply.inc"
#include "ParseSILInstructionVisitorImpl/Phase3_1_AllocDealloc.inc"
#include "ParseSILInstructionVisitorImpl/Phase3_2_AllocDealloc.inc"
#include "ParseSILInstructionVisitorImpl/Phase3_3_StructEnum.inc"
#include "ParseSILInstructionVisitorImpl/Phase3_4_LoadStore.inc"
#include "ParseSILInstructionVisitorImpl/Phase3_5_CopyDestroy.inc"
#include "ParseSILInstructionVisitorImpl/Phase3_6_MoveWrapper.inc"
#include "ParseSILInstructionVisitorImpl/Phase3_7_Conversion.inc"
#include "ParseSILInstructionVisitorImpl/Phase3_8_ControlFlow.inc"
#include "ParseSILInstructionVisitorImpl/Phase3_9_Casts.inc"
