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
