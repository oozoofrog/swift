//===--- Phase1_Alloc.inc - Phase 1 Visit Methods ----------------*- C++ -*-===//
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
// Phase 1: AllocBoxInst (1 instruction)
//
// This file contains visitor method implementations for Phase 1 of the
// SIL instruction parser refactoring. Phase 1 was the validation phase,
// implementing a single instruction to prove the visitor pattern works.
//
// Related GitHub Issue: swift#42962 (SR-340)
//
//===----------------------------------------------------------------------===//

// This file is included by ParseSILInstructionVisitor.cpp
// Do not include this file directly.

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE1_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE1_INC

//===----------------------------------------------------------------------===//
// Phase 1: AllocBoxInst (Validation Phase)
//===----------------------------------------------------------------------===//

// AllocBoxInst - Migrated from ParseSIL.cpp
// Full implementation with all attributes: dynamic_lifetime, reflection,
// moveable_value_debuginfo, pointer_escape
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

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE1_INC
