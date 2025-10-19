//===--- Phase2_Apply.inc - Phase 2 Visit Methods ----------------*- C++ -*-===//
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
// Phase 2: Basic Instructions (14 instructions)
//   - Apply family (6): Apply, BeginApply, PartialApply, TryApply, AbortApply, EndApply
//   - Tuple family (5): Tuple, TupleExtract, TupleElementAddr, TuplePackExtract, TuplePackElementAddr
//   - Metatype family (3): Metatype, ValueMetatype, ExistentialMetatype
//
// Related GitHub Issue: swift#42962 (SR-340)
//
//===----------------------------------------------------------------------===//

// This file is included by ParseSILInstructionVisitor.cpp
// Do not include this file directly.

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE2_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE2_INC

//===----------------------------------------------------------------------===//
// Phase 2: Apply-family instructions (6 instructions)
//===----------------------------------------------------------------------===//

// ApplyInst, BeginApplyInst, PartialApplyInst, TryApplyInst
// These four instructions share the same parsing logic via parseCallInstruction
bool SILInstructionParserVisitor::visitApplyInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  return P.parseCallInstruction(InstLoc, SILInstructionKind::ApplyInst, B,
                                ResultVal);
}

bool SILInstructionParserVisitor::visitBeginApplyInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  return P.parseCallInstruction(InstLoc, SILInstructionKind::BeginApplyInst, B,
                                ResultVal);
}

bool SILInstructionParserVisitor::visitPartialApplyInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  return P.parseCallInstruction(InstLoc, SILInstructionKind::PartialApplyInst,
                                B, ResultVal);
}

bool SILInstructionParserVisitor::visitTryApplyInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  return P.parseCallInstruction(InstLoc, SILInstructionKind::TryApplyInst, B,
                                ResultVal);
}

// AbortApplyInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAbortApplyInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  SILParser::UnresolvedValueName argName;
  if (P.parseValueName(argName))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  SILType expectedTy = SILType::getSILTokenType(P.P.Context);
  SILValue op = P.getLocalValue(argName, expectedTy, InstLoc, B);
  ResultVal = B.createAbortApply(InstLoc, op);
  return false; // Success
}

// EndApplyInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitEndApplyInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  SILParser::UnresolvedValueName argName;
  SILType ResultTy;

  if (P.parseValueName(argName) || P.parseVerbatim("as") ||
      P.parseSILType(ResultTy) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  SILType expectedTy = SILType::getSILTokenType(P.P.Context);
  SILValue op = P.getLocalValue(argName, expectedTy, InstLoc, B);

  ResultVal = B.createEndApply(InstLoc, op, ResultTy);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Phase 2: Tuple-family instructions (5 instructions)
//===----------------------------------------------------------------------===//

// TupleInst - Migrated from ParseSIL.cpp (complex, two syntaxes)
bool SILInstructionParserVisitor::visitTupleInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SmallVector<SILValue, 4> OpList;
  SILValue Val;
  bool HadError = false;

  // Tuple instructions have two different syntaxes, one for simple tuple
  // types, one for complicated ones.
  if (P.P.Tok.isNot(tok::sil_dollar)) {
    // If there is no type, parse the simple form.
    if (P.P.parseToken(tok::l_paren, diag::expected_tok_in_sil_instr, "("))
      return true;

    // This form is used with tuples that have elements with no names or
    // default values.
    SmallVector<TupleTypeElt, 4> TypeElts;
    if (P.P.Tok.isNot(tok::r_paren)) {
      do {
        if (P.parseTypedValueRef(Val, B))
          return true;
        OpList.push_back(Val);
        TypeElts.push_back(Val->getType().getRawASTType());
      } while (P.P.consumeIf(tok::comma));
    }
    HadError |=
        P.P.parseToken(tok::r_paren, diag::expected_tok_in_sil_instr, ")");

    auto Ty = TupleType::get(TypeElts, P.P.Context);
    auto Ty2 = SILType::getPrimitiveObjectType(Ty->getCanonicalType());

    ValueOwnershipKind forwardingOwnership =
        P.F && P.F->hasOwnership() ? getSILValueOwnership(OpList)
                                   : ValueOwnershipKind(OwnershipKind::None);

    if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
        P.parseSILDebugLocation(InstLoc, B))
      return true;

    ResultVal = B.createTuple(InstLoc, Ty2, OpList, forwardingOwnership);
    return HadError;
  }

  // Otherwise, parse the fully general form.
  SILType Ty;
  if (P.parseSILType(Ty) ||
      P.P.parseToken(tok::l_paren, diag::expected_tok_in_sil_instr, "("))
    return true;

  TupleType *TT = Ty.getAs<TupleType>();
  if (TT == nullptr) {
    P.P.diagnose(OpcodeLoc, diag::expected_tuple_type_in_tuple);
    return true;
  }

  SmallVector<TupleTypeElt, 4> TypeElts;
  if (P.P.Tok.isNot(tok::r_paren)) {
    do {
      if (TypeElts.size() > TT->getNumElements()) {
        P.P.diagnose(P.P.Tok, diag::sil_tuple_inst_wrong_value_count,
                     TT->getNumElements());
        return true;
      }
      Type EltTy = TT->getElement(TypeElts.size()).getType();
      if (P.parseValueRef(
              Val, SILType::getPrimitiveObjectType(EltTy->getCanonicalType()),
              RegularLocation(P.P.Tok.getLoc()), B))
        return true;
      OpList.push_back(Val);
      TypeElts.push_back(Val->getType().getRawASTType());
    } while (P.P.consumeIf(tok::comma));
  }
  HadError |=
      P.P.parseToken(tok::r_paren, diag::expected_tok_in_sil_instr, ")");

  if (TypeElts.size() != TT->getNumElements()) {
    P.P.diagnose(OpcodeLoc, diag::sil_tuple_inst_wrong_value_count,
                 TT->getNumElements());
    return true;
  }

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;
  ResultVal = B.createTuple(InstLoc, Ty, OpList);
  return HadError;
}

// TupleExtractInst and TupleElementAddrInst share implementation
bool SILInstructionParserVisitor::visitTupleExtractInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ","))
    return true;

  unsigned Field = 0;
  TupleType *TT = Val->getType().castTo<TupleType>();
  if (P.P.Tok.isNot(tok::integer_literal) ||
      P.parseIntegerLiteral(P.P.Tok.getText(), 10, Field) ||
      Field >= TT->getNumElements()) {
    P.P.diagnose(P.P.Tok, diag::sil_tuple_inst_wrong_field);
    return true;
  }
  P.P.consumeToken(tok::integer_literal);
  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();

  if (P.parseForwardingOwnershipKind(forwardingOwnership))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto ResultTy = TT->getElement(Field).getType()->getCanonicalType();
  ResultVal = B.createTupleExtract(InstLoc, Val, Field,
                                   SILType::getPrimitiveObjectType(ResultTy),
                                   forwardingOwnership);
  return false; // Success
}

bool SILInstructionParserVisitor::visitTupleElementAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ","))
    return true;

  unsigned Field = 0;
  TupleType *TT = Val->getType().castTo<TupleType>();
  if (P.P.Tok.isNot(tok::integer_literal) ||
      P.parseIntegerLiteral(P.P.Tok.getText(), 10, Field) ||
      Field >= TT->getNumElements()) {
    P.P.diagnose(P.P.Tok, diag::sil_tuple_inst_wrong_field);
    return true;
  }
  P.P.consumeToken(tok::integer_literal);

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto ResultTy = TT->getElement(Field).getType()->getCanonicalType();
  ResultVal =
      B.createTupleElementAddr(InstLoc, Val, Field,
                               SILType::getPrimitiveAddressType(ResultTy));
  return false; // Success
}

// TuplePackExtractInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitTuplePackExtractInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue index, tuple;
  SILType elementType;

  if (P.parseValueRef(index, SILType::getPackIndexType(P.P.Context), InstLoc,
                      B) ||
      P.parseVerbatim("of") || P.parseTypedValueRef(tuple, B) ||
      P.parseVerbatim("as") || P.parseSILType(elementType))
    return true;

  ResultVal = B.createTuplePackExtract(InstLoc, index, tuple, elementType);
  return false; // Success
}

// TuplePackElementAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitTuplePackElementAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue index, tuple;
  SILType elementType;

  if (P.parseValueRef(index, SILType::getPackIndexType(P.P.Context), InstLoc,
                      B) ||
      P.parseVerbatim("of") || P.parseTypedValueRef(tuple, B) ||
      P.parseVerbatim("as") || P.parseSILType(elementType))
    return true;

  ResultVal = B.createTuplePackElementAddr(InstLoc, index, tuple, elementType);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Phase 2: Metatype-family instructions (3 instructions)
//===----------------------------------------------------------------------===//

// MetatypeInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitMetatypeInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;

  if (P.parseSILType(Ty))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createMetatype(InstLoc, Ty);
  return false; // Success
}

// ValueMetatypeInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitValueMetatypeInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;
  SILValue Val;

  if (P.parseSILType(Ty) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createValueMetatype(InstLoc, Ty, Val);
  return false; // Success
}

// ExistentialMetatypeInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitExistentialMetatypeInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;
  SILValue Val;

  if (P.parseSILType(Ty) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createExistentialMetatype(InstLoc, Ty, Val);
  return false; // Success
}


#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE2_INC
