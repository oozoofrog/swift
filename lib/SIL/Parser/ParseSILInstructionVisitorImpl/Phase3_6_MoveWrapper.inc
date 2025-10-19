//===--- Phase3_6_MoveWrapper.inc - Phase 3-6 Visit Methods --------*- C++ -*-===//
//
// Phase 3-6: Move/Wrapper Instructions (6 instructions)
//   - MoveValue, MarkUnresolvedNonCopyableValue
//   - CopyableToMoveOnlyWrapperValue/Addr
//   - MoveOnlyWrapperToCopyableValue/Addr
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE6_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE6_INC

//===----------------------------------------------------------------------===//
// Phase 3-6: Move/Wrapper Instructions
//===----------------------------------------------------------------------===//

// MoveValueInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitMoveValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool allowsDiagnostics = false;
  auto isLexical = IsNotLexical;
  auto hasPointerEscape = DoesNotHavePointerEscape;
  auto fromVarDecl = IsNotFromVarDecl;

  StringRef AttrName;
  SourceLoc AttrLoc;
  while (parseSILOptional(AttrName, AttrLoc, P)) {
    if (AttrName == "allows_diagnostics")
      allowsDiagnostics = true;
    else if (AttrName == "lexical")
      isLexical = IsLexical;
    else if (AttrName == "pointer_escape")
      hasPointerEscape = HasPointerEscape;
    else if (AttrName == "var_decl")
      fromVarDecl = IsFromVarDecl;
    else {
      P.P.diagnose(InstLoc.getSourceLoc(),
                   diag::sil_invalid_attribute_for_instruction, AttrName,
                   "move_value");
      return true;
    }
  }

  SILValue Val;
  if (P.parseTypedValueRef(Val, B))
    return true;
  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto *MVI = B.createMoveValue(InstLoc, Val, isLexical, hasPointerEscape,
                                fromVarDecl);
  MVI->setAllowsDiagnostics(allowsDiagnostics);
  ResultVal = MVI;
  return false; // Success
}

// MarkUnresolvedNonCopyableValueInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitMarkUnresolvedNonCopyableValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  StringRef AttrName;
  SourceLoc AttrLoc;
  if (!parseSILOptional(AttrName, AttrLoc, P)) {
    auto diag = diag::sil_markmustcheck_requires_attribute;
    P.P.diagnose(InstLoc.getSourceLoc(), diag);
    return true;
  }

  auto Strict = MarkUnresolvedNonCopyableValueInst::IsNotStrict;
  if (AttrName == "strict") {
    Strict = MarkUnresolvedNonCopyableValueInst::IsStrict;
    if (!parseSILOptional(AttrName, AttrLoc, P)) {
      auto diag = diag::sil_markmustcheck_requires_attribute;
      P.P.diagnose(InstLoc.getSourceLoc(), diag);
      return true;
    }
  }

  using CheckKind = MarkUnresolvedNonCopyableValueInst::CheckKind;
  CheckKind CKind =
      llvm::StringSwitch<CheckKind>(AttrName)
          .Case("consumable_and_assignable",
                CheckKind::ConsumableAndAssignable)
          .Case("no_consume_or_assign", CheckKind::NoConsumeOrAssign)
          .Case("assignable_but_not_consumable",
                CheckKind::AssignableButNotConsumable)
          .Case("initable_but_not_consumable",
                CheckKind::InitableButNotConsumable)
          .Default(CheckKind::Invalid);

  if (CKind == CheckKind::Invalid) {
    auto diag = diag::sil_markmustcheck_invalid_attribute;
    P.P.diagnose(InstLoc.getSourceLoc(), diag, AttrName);
    return true;
  }

  SILValue Val;
  if (P.parseTypedValueRef(Val, B))
    return true;
  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto *MVI = B.createMarkUnresolvedNonCopyableValueInst(InstLoc, Val, CKind,
                                                         Strict);
  ResultVal = MVI;
  return false; // Success
}

// CopyableToMoveOnlyWrapperValueInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitCopyableToMoveOnlyWrapperValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  StringRef AttrName;
  SourceLoc AttrLoc;
  if (!parseSILOptional(AttrName, AttrLoc, P)) {
    auto diag = diag::sil_moveonlytocopyable_requires_attribute;
    P.P.diagnose(InstLoc.getSourceLoc(), diag);
    return true;
  }

  OwnershipKind OwnershipKind =
      llvm::StringSwitch<ValueOwnershipKind>(AttrName)
          .Case("owned", OwnershipKind::Owned)
          .Case("guaranteed", OwnershipKind::Guaranteed)
          .Default(OwnershipKind::None);

  if (OwnershipKind == OwnershipKind::None) {
    auto diag = diag::sil_moveonlytocopyable_invalid_attribute;
    P.P.diagnose(InstLoc.getSourceLoc(), diag, AttrName);
    return true;
  }

  SILValue Val;
  if (P.parseTypedValueRef(Val, B))
    return true;
  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!Val->getType().isObject()) {
    P.P.diagnose(InstLoc.getSourceLoc(),
                 diag::sil_operand_not_object, "operand", OpcodeName);
    return true;
  }

  if (Val->getType().isMoveOnlyWrapped()) {
    P.P.diagnose(InstLoc.getSourceLoc(),
                 diag::sil_operand_has_incorrect_moveonlywrapped,
                 "operand", OpcodeName, 1);
    return true;
  }

  if (OwnershipKind == OwnershipKind::Owned)
    ResultVal = B.createOwnedCopyableToMoveOnlyWrapperValue(InstLoc, Val);
  else
    ResultVal =
        B.createGuaranteedCopyableToMoveOnlyWrapperValue(InstLoc, Val);
  return false; // Success
}

// MoveOnlyWrapperToCopyableValueInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitMoveOnlyWrapperToCopyableValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  StringRef AttrName;
  SourceLoc AttrLoc;
  if (!parseSILOptional(AttrName, AttrLoc, P)) {
    auto diag = diag::sil_moveonlytocopyable_requires_attribute;
    P.P.diagnose(InstLoc.getSourceLoc(), diag);
    return true;
  }

  OwnershipKind OwnershipKind =
      llvm::StringSwitch<ValueOwnershipKind>(AttrName)
          .Case("owned", OwnershipKind::Owned)
          .Case("guaranteed", OwnershipKind::Guaranteed)
          .Default(OwnershipKind::None);

  if (OwnershipKind == OwnershipKind::None) {
    auto diag = diag::sil_moveonlytocopyable_invalid_attribute;
    P.P.diagnose(InstLoc.getSourceLoc(), diag, AttrName);
    return true;
  }

  SILValue Val;
  if (P.parseTypedValueRef(Val, B))
    return true;
  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!Val->getType().isObject()) {
    P.P.diagnose(InstLoc.getSourceLoc(),
                 diag::sil_operand_not_object, "operand", OpcodeName);
    return true;
  }

  if (!Val->getType().isMoveOnlyWrapped()) {
    P.P.diagnose(InstLoc.getSourceLoc(),
                 diag::sil_operand_has_incorrect_moveonlywrapped,
                 "operand", OpcodeName, 0);
    return true;
  }

  if (OwnershipKind == OwnershipKind::Owned)
    ResultVal = B.createOwnedMoveOnlyWrapperToCopyableValue(InstLoc, Val);
  else
    ResultVal = B.createGuaranteedMoveOnlyWrapperToCopyableValue(InstLoc, Val);
  return false; // Success
}

// MoveOnlyWrapperToCopyableAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitMoveOnlyWrapperToCopyableAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue addrVal;
  SourceLoc addrLoc;

  if (P.parseTypedValueRef(addrVal, addrLoc, B))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!addrVal->getType().isAddress()) {
    P.P.diagnose(addrLoc, diag::sil_operand_not_address, "operand", OpcodeName);
    return true;
  }

  if (!addrVal->getType().isMoveOnlyWrapped()) {
    P.P.diagnose(addrLoc, diag::sil_operand_has_incorrect_moveonlywrapped,
                 "operand", OpcodeName, 0);
    return true;
  }

  ResultVal = B.createMoveOnlyWrapperToCopyableAddr(InstLoc, addrVal);
  return false; // Success
}

// CopyableToMoveOnlyWrapperAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitCopyableToMoveOnlyWrapperAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue addrVal;
  SourceLoc addrLoc;

  if (P.parseTypedValueRef(addrVal, addrLoc, B))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!addrVal->getType().isAddress()) {
    P.P.diagnose(addrLoc, diag::sil_operand_not_address, "operand", OpcodeName);
    return true;
  }

  if (addrVal->getType().isMoveOnlyWrapped()) {
    P.P.diagnose(addrLoc, diag::sil_operand_has_incorrect_moveonlywrapped,
                 "operand", OpcodeName, 1);
    return true;
  }

  ResultVal = B.createCopyableToMoveOnlyWrapperAddr(InstLoc, addrVal);
  return false; // Success
}


#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE6_INC
