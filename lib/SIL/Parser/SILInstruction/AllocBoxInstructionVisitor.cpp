//===--- SILInstructionVisitor.h - SIL Instruction Visitor ----------------===//
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

#include "SILInstructionVisitor.h"
#include "SILParser.h"
#include "swift/AST/Attr.h"
#include "swift/SIL/SILType.h"

namespace swift {

VisitResult AllocBoxInstructionVisitor::visit(SILBuilder &B, SILLocation &InstLoc, SILParser &P, SILInstruction *&ResultVal) {
    auto hasDynamicLifetime = DoesNotHaveDynamicLifetime;
    bool hasReflection = false;
    UsesMoveableValueDebugInfo_t usesMoveableValueDebugInfo =
        DoesNotUseMoveableValueDebugInfo;
    auto hasPointerEscape = DoesNotHavePointerEscape;
    StringRef attrName;
    SourceLoc attrLoc;
    while (P.parseSILOptional(attrName, attrLoc, *this)) {
      if (attrName == "dynamic_lifetime") {
        hasDynamicLifetime = HasDynamicLifetime;
      } else if (attrName == "reflection") {
        hasReflection = true;
      } else if (attrName == "moveable_value_debuginfo") {
        usesMoveableValueDebugInfo = UsesMoveableValueDebugInfo;
      } else if (attrName == "pointer_escape") {
        hasPointerEscape = HasPointerEscape;
      } else {
        P.diagnose(attrLoc, diag::sil_invalid_attribute_for_expected, attrName,
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
    break;
}

}