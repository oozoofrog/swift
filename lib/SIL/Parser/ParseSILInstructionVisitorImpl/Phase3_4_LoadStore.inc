//===--- Phase3_4_LoadStore.inc - Phase 3-4 Visit Methods ----------*- C++ -*-===//
//
// Phase 3-4: Load/Store Basic Instructions (4 instructions)
//   - Load, LoadBorrow, Store, StoreBorrow
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE4_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE4_INC

//===----------------------------------------------------------------------===//
// Phase 3-4: Load/Store Basic Instructions
//===----------------------------------------------------------------------===//

// LoadInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitLoadInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  std::optional<LoadOwnershipQualifier> Qualifier;
  SourceLoc AddrLoc;
  SILValue Val;

  auto parseLoadOwnership = [](StringRef Str) {
    return llvm::StringSwitch<std::optional<LoadOwnershipQualifier>>(Str)
        .Case("take", LoadOwnershipQualifier::Take)
        .Case("copy", LoadOwnershipQualifier::Copy)
        .Case("trivial", LoadOwnershipQualifier::Trivial)
        .Default(std::nullopt);
  };

  if (P.parseSILQualifier<LoadOwnershipQualifier>(Qualifier, parseLoadOwnership) ||
      P.parseTypedValueRef(Val, AddrLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!Qualifier)
    Qualifier = LoadOwnershipQualifier::Unqualified;

  ResultVal = B.createLoad(InstLoc, Val, Qualifier.value());
  return false; // Success
}

// LoadBorrowInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitLoadBorrowInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SourceLoc AddrLoc;
  SILValue Val;
  bool IsUnchecked = false;
  StringRef AttrName;
  SourceLoc AttrLoc;

  if (parseSILOptional(AttrName, AttrLoc, P)) {
    if (AttrName == "unchecked") {
      IsUnchecked = true;
    } else {
      P.P.diagnose(InstLoc.getSourceLoc(),
                   diag::sil_invalid_attribute_for_instruction, AttrName,
                   "load_borrow");
      return true;
    }
  }

  if (P.parseTypedValueRef(Val, AddrLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto LB = B.createLoadBorrow(InstLoc, Val);
  LB->setUnchecked(IsUnchecked);
  ResultVal = LB;
  return false; // Success
}

// StoreInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitStoreInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILParser::UnresolvedValueName From;
  SourceLoc ToLoc, AddrLoc;
  Identifier ToToken;
  SILValue AddrVal;
  std::optional<StoreOwnershipQualifier> StoreQualifier;

  if (P.parseValueName(From) ||
      P.parseSILIdentifier(ToToken, ToLoc, diag::expected_tok_in_sil_instr,
                           "to"))
    return true;

  auto parseStoreOwnership = [](StringRef Str) {
    return llvm::StringSwitch<std::optional<StoreOwnershipQualifier>>(Str)
        .Case("init", StoreOwnershipQualifier::Init)
        .Case("assign", StoreOwnershipQualifier::Assign)
        .Case("trivial", StoreOwnershipQualifier::Trivial)
        .Default(std::nullopt);
  };

  if (P.parseSILQualifier<StoreOwnershipQualifier>(StoreQualifier,
                                                   parseStoreOwnership))
    return true;

  if (P.parseTypedValueRef(AddrVal, AddrLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (ToToken.str() != "to") {
    P.P.diagnose(ToLoc, diag::expected_tok_in_sil_instr, "to");
    return true;
  }

  if (!AddrVal->getType().isAddress()) {
    P.P.diagnose(AddrLoc, diag::sil_operand_not_address, "destination",
                 OpcodeName);
    return true;
  }

  SILType ValType = AddrVal->getType().getObjectType();

  if (!StoreQualifier)
    StoreQualifier = StoreOwnershipQualifier::Unqualified;

  ResultVal =
      B.createStore(InstLoc, P.getLocalValue(From, ValType, InstLoc, B),
                    AddrVal, StoreQualifier.value());
  return false; // Success
}

// StoreBorrowInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitStoreBorrowInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILParser::UnresolvedValueName from;
  SourceLoc toLoc, addrLoc;
  Identifier toToken;
  SILValue addrVal;

  if (P.parseValueName(from) ||
      P.parseSILIdentifier(toToken, toLoc, diag::expected_tok_in_sil_instr,
                           "to") ||
      P.parseTypedValueRef(addrVal, addrLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (toToken.str() != "to") {
    P.P.diagnose(toLoc, diag::expected_tok_in_sil_instr, "to");
    return true;
  }

  if (!addrVal->getType().isAddress()) {
    P.P.diagnose(addrLoc, diag::sil_operand_not_address, "destination",
                 OpcodeName);
    return true;
  }

  SILType valueTy = addrVal->getType().getObjectType();
  ResultVal = B.createStoreBorrow(
      InstLoc, P.getLocalValue(from, valueTy, InstLoc, B), addrVal);
  return false; // Success
}


#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE4_INC
