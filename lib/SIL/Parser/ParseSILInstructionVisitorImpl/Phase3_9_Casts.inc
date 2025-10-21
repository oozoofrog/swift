//===--- Phase3_9_Casts.inc - Phase 3-9 Visit Methods ---------*- C++ -*-===//
//
// Phase 3-9: Additional Cast/Conversion instructions (18 instructions)
//   - Address/Pointer conversions
//   - Reference storage conversions
//   - Function/Objective-C metatype conversions
//   - Bridge object utilities
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE39_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE39_INC

namespace {

static bool parseValueToTypeOperand(SILParser &SP, SILBuilder &B,
                                    SILValue &Val, Identifier &ToToken,
                                    SourceLoc &ToLoc, SILType &Ty) {
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

  return false;
}

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Address/Pointer conversions
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitUncheckedAddrCastInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ToToken;
  SourceLoc ToLoc;
  SILType Ty;

  if (parseValueToTypeOperand(P, B, Val, ToToken, ToLoc, Ty) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createUncheckedAddrCast(InstLoc, Val, Ty);
  return false; // Success
}

bool SILInstructionParserVisitor::visitUncheckedValueCastInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ToToken;
  SourceLoc ToLoc;
  SILType Ty;

  if (parseValueToTypeOperand(P, B, Val, ToToken, ToLoc, Ty))
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createUncheckedValueCast(InstLoc, Val, Ty, forwardingOwnership);
  return false; // Success
}

bool SILInstructionParserVisitor::visitAddressToPointerInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ToToken;
  SourceLoc ToLoc;
  bool needsStackProtection = false;

  if (parseSILOptional(needsStackProtection, P, "stack_protection"))
    ; // attribute consumed

  if (P.parseTypedValueRef(Val, B) ||
      P.parseSILIdentifier(ToToken, ToLoc, diag::expected_tok_in_sil_instr,
                           "to"))
    return true;

  if (ToToken.str() != "to") {
    P.P.diagnose(ToLoc, diag::expected_tok_in_sil_instr, "to");
    return true;
  }

  SILType Ty;
  if (P.parseSILType(Ty) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createAddressToPointer(InstLoc, Val, Ty, needsStackProtection);
  return false; // Success
}

bool SILInstructionParserVisitor::visitRawPointerToRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ToToken;
  SourceLoc ToLoc;
  SILType Ty;

  if (parseValueToTypeOperand(P, B, Val, ToToken, ToLoc, Ty) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createRawPointerToRef(InstLoc, Val, Ty);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Reference storage conversions
//===----------------------------------------------------------------------===//

#define LOADABLE_REF_STORAGE(Name, ...)                                        \
bool SILInstructionParserVisitor::visitRefTo##Name##Inst() {                   \
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);        \
  SILValue Val;                                                                \
  Identifier ToToken;                                                          \
  SourceLoc ToLoc;                                                             \
  SILType Ty;                                                                  \
  if (parseValueToTypeOperand(P, B, Val, ToToken, ToLoc, Ty) ||                \
      P.parseSILDebugLocation(InstLoc, B))                                     \
    return true;                                                               \
  ResultVal = B.createRefTo##Name(InstLoc, Val, Ty);                           \
  return false;                                                                \
}                                                                              \
                                                                               \
bool SILInstructionParserVisitor::visit##Name##ToRefInst() {                   \
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);        \
  SILValue Val;                                                                \
  Identifier ToToken;                                                          \
  SourceLoc ToLoc;                                                             \
  SILType Ty;                                                                  \
  if (parseValueToTypeOperand(P, B, Val, ToToken, ToLoc, Ty) ||                \
      P.parseSILDebugLocation(InstLoc, B))                                     \
    return true;                                                               \
  ResultVal = B.create##Name##ToRef(InstLoc, Val, Ty);                         \
  return false;                                                                \
}
#include "swift/AST/ReferenceStorage.def"
#undef LOADABLE_REF_STORAGE

//===----------------------------------------------------------------------===//
// Function / Objective-C metatype conversions
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitThinToThickFunctionInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ToToken;
  SourceLoc ToLoc;
  SILType Ty;

  if (parseValueToTypeOperand(P, B, Val, ToToken, ToLoc, Ty))
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createThinToThickFunction(InstLoc, Val, Ty, forwardingOwnership);
  return false; // Success
}

bool SILInstructionParserVisitor::visitThickToObjCMetatypeInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ToToken;
  SourceLoc ToLoc;
  SILType Ty;

  if (parseValueToTypeOperand(P, B, Val, ToToken, ToLoc, Ty) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createThickToObjCMetatype(InstLoc, Val, Ty);
  return false; // Success
}

bool SILInstructionParserVisitor::visitObjCToThickMetatypeInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ToToken;
  SourceLoc ToLoc;
  SILType Ty;

  if (parseValueToTypeOperand(P, B, Val, ToToken, ToLoc, Ty) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createObjCToThickMetatype(InstLoc, Val, Ty);
  return false; // Success
}

bool SILInstructionParserVisitor::visitConvertEscapeToNoEscapeInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ToToken;
  SourceLoc ToLoc;
  SILType Ty;
  bool notGuaranteed = false;

  StringRef AttrName;
  SourceLoc AttrLoc;
  if (parseSILOptional(AttrName, AttrLoc, P)) {
    if (AttrName == "not_guaranteed") {
      notGuaranteed = true;
    } else {
      P.P.diagnose(AttrLoc, diag::sil_invalid_attribute_for_instruction,
                   AttrName, "convert_escape_to_noescape");
      return true;
    }
  }

  if (parseValueToTypeOperand(P, B, Val, ToToken, ToLoc, Ty))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createConvertEscapeToNoEscape(InstLoc, Val, Ty, !notGuaranteed);
  return false; // Success
}

bool SILInstructionParserVisitor::visitObjCMetatypeToObjectInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ToToken;
  SourceLoc ToLoc;
  SILType Ty;

  if (parseValueToTypeOperand(P, B, Val, ToToken, ToLoc, Ty) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createObjCMetatypeToObject(InstLoc, Val, Ty);
  return false; // Success
}

bool SILInstructionParserVisitor::visitObjCExistentialMetatypeToObjectInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ToToken;
  SourceLoc ToLoc;
  SILType Ty;

  if (parseValueToTypeOperand(P, B, Val, ToToken, ToLoc, Ty) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createObjCExistentialMetatypeToObject(InstLoc, Val, Ty);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Miscellaneous conversions
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitPointerToAddressInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ToToken;
  SourceLoc ToLoc;
  bool isStrict = false;
  bool isInvariant = false;
  llvm::MaybeAlign alignment;

  if (P.parseTypedValueRef(Val, B) ||
      P.parseSILIdentifier(ToToken, ToLoc, diag::expected_tok_in_sil_instr,
                           "to"))
    return true;

  while (P.P.consumeIf(tok::l_square)) {
    Identifier AttrId;
    SourceLoc AttrLoc;
    if (P.parseSILIdentifier(AttrId, AttrLoc,
                             diag::expected_in_attribute_list))
      return true;

    StringRef AttrName = AttrId.str();
    if (AttrName == "strict") {
      isStrict = true;
    } else if (AttrName == "invariant") {
      isInvariant = true;
    } else if (AttrName == "align") {
      if (!P.P.Tok.is(tok::integer_literal)) {
        P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr,
                     "integer literal");
        return true;
      }
      uint64_t alignValue = 0;
      if (P.parseIntegerLiteral(P.P.Tok.getText(), /*radix*/ 0, alignValue)) {
        P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr,
                     "integer literal");
        return true;
      }
      P.P.consumeToken(tok::integer_literal);
      alignment = llvm::Align(alignValue);
    } else {
      P.P.diagnose(AttrLoc, diag::sil_invalid_attribute_for_instruction,
                   AttrName, "pointer_to_address");
      return true;
    }

    if (P.P.parseToken(tok::r_square, diag::expected_in_attribute_list))
      return true;
  }

  if (ToToken.str() != "to") {
    P.P.diagnose(ToLoc, diag::expected_tok_in_sil_instr, "to");
    return true;
  }

  SILType Ty;
  if (P.parseSILType(Ty) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createPointerToAddress(InstLoc, Val, Ty, isStrict,
                                       isInvariant, alignment);
  return false; // Success
}

bool SILInstructionParserVisitor::visitRefToBridgeObjectInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILValue BitsVal;

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseTypedValueRef(BitsVal, B))
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createRefToBridgeObject(InstLoc, Val, BitsVal,
                                        forwardingOwnership);
  return false; // Success
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE39_INC
