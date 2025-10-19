//===--- Phase3_5_CopyDestroy.inc - Phase 3-5 Visit Methods --------*- C++ -*-===//
//
// Phase 3-5: Copy/Destroy/Move Instructions (5 instructions)
//   - DestroyValue, DestroyNotEscapedClosure
//   - CopyAddr, ExplicitCopyAddr, MarkUnresolvedMoveAddr
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE5_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE5_INC

//===----------------------------------------------------------------------===//
// Phase 3-5: Copy/Destroy/Move Instructions
//===----------------------------------------------------------------------===//

// DestroyValueInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDestroyValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  PoisonRefs_t poisonRefs = DontPoisonRefs;
  IsDeadEnd_t isDeadEnd = IsntDeadEnd;
  StringRef attributeName;
  SourceLoc attributeLoc;

  while (parseSILOptional(attributeName, attributeLoc, P)) {
    if (attributeName == "poison")
      poisonRefs = PoisonRefs;
    else if (attributeName == "dead_end")
      isDeadEnd = IsDeadEnd;
    else {
      P.P.diagnose(attributeLoc, diag::sil_invalid_attribute_for_instruction,
                   attributeName, "destroy_value");
      return true;
    }
  }

  SILValue Val;
  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDestroyValue(InstLoc, Val, poisonRefs, isDeadEnd);
  return false; // Success
}

// DestroyNotEscapedClosureInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDestroyNotEscapedClosureInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool IsObjcVerificationType = false;

  if (parseSILOptional(IsObjcVerificationType, P, "objc"))
    return true;

  SILValue Val;
  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDestroyNotEscapedClosure(
      InstLoc, Val,
      IsObjcVerificationType ? DestroyNotEscapedClosureInst::ObjCEscaping
                             : DestroyNotEscapedClosureInst::WithoutActuallyEscaping);
  return false; // Success
}

// CopyAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitCopyAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool IsTake = false, IsInit = false;
  SILParser::UnresolvedValueName SrcLName;
  SILValue DestLVal;
  SourceLoc ToLoc, DestLoc;
  Identifier ToToken;

  if (parseSILOptional(IsTake, P, "take") || P.parseValueName(SrcLName) ||
      P.parseSILIdentifier(ToToken, ToLoc, diag::expected_tok_in_sil_instr,
                           "to") ||
      parseSILOptional(IsInit, P, "init") ||
      P.parseTypedValueRef(DestLVal, DestLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (ToToken.str() != "to") {
    P.P.diagnose(ToLoc, diag::expected_tok_in_sil_instr, "to");
    return true;
  }

  if (!DestLVal->getType().isAddress()) {
    P.P.diagnose(DestLoc, diag::sil_invalid_instr_operands);
    return true;
  }

  SILValue SrcLVal =
      P.getLocalValue(SrcLName, DestLVal->getType(), InstLoc, B);
  ResultVal = B.createCopyAddr(InstLoc, SrcLVal, DestLVal, IsTake_t(IsTake),
                                IsInitialization_t(IsInit));
  return false; // Success
}

// ExplicitCopyAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitExplicitCopyAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool IsTake = false, IsInit = false;
  SILParser::UnresolvedValueName SrcLName;
  SILValue DestLVal;
  SourceLoc ToLoc, DestLoc;
  Identifier ToToken;

  if (parseSILOptional(IsTake, P, "take") || P.parseValueName(SrcLName) ||
      P.parseSILIdentifier(ToToken, ToLoc, diag::expected_tok_in_sil_instr,
                           "to") ||
      parseSILOptional(IsInit, P, "init") ||
      P.parseTypedValueRef(DestLVal, DestLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (ToToken.str() != "to") {
    P.P.diagnose(ToLoc, diag::expected_tok_in_sil_instr, "to");
    return true;
  }

  if (!DestLVal->getType().isAddress()) {
    P.P.diagnose(DestLoc, diag::sil_invalid_instr_operands);
    return true;
  }

  SILValue SrcLVal =
      P.getLocalValue(SrcLName, DestLVal->getType(), InstLoc, B);
  ResultVal =
      B.createExplicitCopyAddr(InstLoc, SrcLVal, DestLVal, IsTake_t(IsTake),
                               IsInitialization_t(IsInit));
  return false; // Success
}

// MarkUnresolvedMoveAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitMarkUnresolvedMoveAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILParser::UnresolvedValueName SrcLName;
  SILValue DestLVal;
  SourceLoc ToLoc, DestLoc;
  Identifier ToToken;

  if (P.parseValueName(SrcLName) ||
      P.parseSILIdentifier(ToToken, ToLoc, diag::expected_tok_in_sil_instr,
                           "to") ||
      P.parseTypedValueRef(DestLVal, DestLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (ToToken.str() != "to") {
    P.P.diagnose(ToLoc, diag::expected_tok_in_sil_instr, "to");
    return true;
  }

  if (!DestLVal->getType().isAddress()) {
    P.P.diagnose(DestLoc, diag::sil_invalid_instr_operands);
    return true;
  }

  SILValue SrcLVal =
      P.getLocalValue(SrcLName, DestLVal->getType(), InstLoc, B);
  ResultVal = B.createMarkUnresolvedMoveAddr(InstLoc, SrcLVal, DestLVal);
  return false; // Success
}


#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE5_INC
