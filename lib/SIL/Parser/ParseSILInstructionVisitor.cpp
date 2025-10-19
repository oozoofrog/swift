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

// Phase 1: Full implementation for AllocBoxInst
// Migrated from ParseSIL.cpp case SILInstructionKind::AllocBoxInst
bool SILInstructionParserVisitor::visitAllocBoxInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  auto hasDynamicLifetime = DoesNotHaveDynamicLifetime;
  bool hasReflection = false;
  UsesMoveableValueDebugInfo_t usesMoveableValueDebugInfo =
      DoesNotUseMoveableValueDebugInfo;
  auto hasPointerEscape = DoesNotHavePointerEscape;
  StringRef attrName;
  SourceLoc attrLoc;

  while (parseSILOptional(attrName, attrLoc, P)) {
    if (attrName == "dynamic_lifetime") {
      hasDynamicLifetime = HasDynamicLifetime;
    } else if (attrName == "reflection") {
      hasReflection = true;
    } else if (attrName == "moveable_value_debuginfo") {
      usesMoveableValueDebugInfo = UsesMoveableValueDebugInfo;
    } else if (attrName == "pointer_escape") {
      hasPointerEscape = HasPointerEscape;
    } else {
      P.P.diagnose(attrLoc, diag::sil_invalid_attribute_for_expected, attrName,
                   "dynamic_lifetime, reflection, pointer_escape or "
                   "usesMoveableValueDebugInfo");
    }
  }

  SILType Ty;
  if (P.parseSILType(Ty))
    return true;
  SILDebugVariable VarInfo;
  if (P.parseSILDebugVar(VarInfo))
    return true;
  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (Ty.isMoveOnly())
    usesMoveableValueDebugInfo = UsesMoveableValueDebugInfo;

  ResultVal = B.createAllocBox(InstLoc, Ty.castTo<SILBoxType>(), VarInfo,
                               hasDynamicLifetime, hasReflection,
                               usesMoveableValueDebugInfo,
                               /*skipVarDeclAssert*/ false, hasPointerEscape);
  return false; // Success
}
