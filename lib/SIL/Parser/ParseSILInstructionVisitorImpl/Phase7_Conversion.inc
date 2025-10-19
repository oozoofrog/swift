//===--- Phase7_Conversion.inc - Phase 7 Visit Methods -----------*- C++ -*-===//
//
// Phase 7: Conversion/Cast Instructions (8 instructions)
//   - Helper: parseValueToType
//   - UncheckedRefCast, UncheckedTrivialBitCast, UncheckedBitwiseCast
//   - ConvertFunction, BridgeObjectToRef, BridgeObjectToWord
//   - Upcast, RefToRawPointer
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE7_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE7_INC

//===----------------------------------------------------------------------===//
// Phase 3-7: Conversion/Cast Instructions
//===----------------------------------------------------------------------===//

// Helper function for parsing "value to type" pattern with optional forwarding ownership
static bool parseValueToType(SILParser &SP, SILBuilder &B,
                              SILLocation InstLoc,
                              SILValue &Val, SILType &Ty,
                              ValueOwnershipKind &forwardingOwnership,
                              bool parseOwnership, SILInstructionKind Opcode) {
  SourceLoc ToLoc;
  Identifier ToToken;

  if (SP.parseTypedValueRef(Val, B) ||
      SP.parseSILIdentifier(ToToken, ToLoc, diag::expected_tok_in_sil_instr,
                           "to"))
    return true;

  if (ToToken.str() != "to") {
    SP.P.diagnose(ToLoc, diag::expected_tok_in_sil_instr, "to");
    return true;
  }

  if (SP.parseSILType(Ty))
    return true;

  if (parseOwnership) {
    forwardingOwnership = Val->getOwnershipKind();
    if (ForwardingInstruction::isa(Opcode)) {
      if (SP.parseForwardingOwnershipKind(forwardingOwnership))
        return true;
    }
  }

  return false;
}

// UncheckedRefCastInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitUncheckedRefCastInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;
  ValueOwnershipKind forwardingOwnership = OwnershipKind::None;

  if (parseValueToType(P, B, InstLoc, Val, Ty, forwardingOwnership, true,
                       SILInstructionKind::UncheckedRefCastInst))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createUncheckedRefCast(InstLoc, Val, Ty, forwardingOwnership);
  return false; // Success
}

// UncheckedTrivialBitCastInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitUncheckedTrivialBitCastInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;
  ValueOwnershipKind forwardingOwnership = OwnershipKind::None;

  if (parseValueToType(P, B, InstLoc, Val, Ty, forwardingOwnership, false,
                       SILInstructionKind::UncheckedTrivialBitCastInst))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createUncheckedTrivialBitCast(InstLoc, Val, Ty);
  return false; // Success
}

// UncheckedBitwiseCastInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitUncheckedBitwiseCastInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;
  ValueOwnershipKind forwardingOwnership = OwnershipKind::None;

  if (parseValueToType(P, B, InstLoc, Val, Ty, forwardingOwnership, false,
                       SILInstructionKind::UncheckedBitwiseCastInst))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createUncheckedBitwiseCast(InstLoc, Val, Ty);
  return false; // Success
}

// ConvertFunctionInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitConvertFunctionInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;
  ValueOwnershipKind forwardingOwnership = OwnershipKind::None;
  bool without_actually_escaping = false;

  if (parseValueToType(P, B, InstLoc, Val, Ty, forwardingOwnership, false,
                       SILInstructionKind::ConvertFunctionInst))
    return true;

  // Parse optional [without_actually_escaping] attribute after type
  StringRef attrName;
  SourceLoc attrLoc;
  if (parseSILOptional(attrName, attrLoc, P)) {
    if (attrName == "without_actually_escaping")
      without_actually_escaping = true;
    else {
      P.P.diagnose(attrLoc, diag::sil_invalid_attribute_for_instruction,
                   attrName, "convert_function");
      return true;
    }
  }

  forwardingOwnership = Val->getOwnershipKind();
  if (ForwardingInstruction::isa(SILInstructionKind::ConvertFunctionInst)) {
    if (P.parseForwardingOwnershipKind(forwardingOwnership))
      return true;
  }

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createConvertFunction(
      InstLoc, Val, Ty, without_actually_escaping, forwardingOwnership);
  return false; // Success
}

// BridgeObjectToRefInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitBridgeObjectToRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;
  ValueOwnershipKind forwardingOwnership = OwnershipKind::None;

  if (parseValueToType(P, B, InstLoc, Val, Ty, forwardingOwnership, true,
                       SILInstructionKind::BridgeObjectToRefInst))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createBridgeObjectToRef(InstLoc, Val, Ty, forwardingOwnership);
  return false; // Success
}

// BridgeObjectToWordInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitBridgeObjectToWordInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createBridgeObjectToWord(InstLoc, Val);
  return false; // Success
}

// UpcastInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitUpcastInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;
  ValueOwnershipKind forwardingOwnership = OwnershipKind::None;

  if (parseValueToType(P, B, InstLoc, Val, Ty, forwardingOwnership, true,
                       SILInstructionKind::UpcastInst))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createUpcast(InstLoc, Val, Ty, forwardingOwnership);
  return false; // Success
}

// RefToRawPointerInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitRefToRawPointerInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;
  ValueOwnershipKind forwardingOwnership = OwnershipKind::None;

  if (parseValueToType(P, B, InstLoc, Val, Ty, forwardingOwnership, false,
                       SILInstructionKind::RefToRawPointerInst))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createRefToRawPointer(InstLoc, Val, Ty);
  return false; // Success
}


#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE7_INC
