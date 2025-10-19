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

// Helper to parse expected optional boolean attributes like [take] or [init]
static bool parseSILOptional(bool &Result, SILParser &SP, StringRef Expected) {
  StringRef Optional;
  SourceLoc Loc;
  if (parseSILOptional(Optional, Loc, SP)) {
    if (Optional != Expected) {
      SP.P.diagnose(Loc, diag::sil_invalid_attribute_for_expected, Optional,
                    Expected);
      return true;
    }
    Result = true;
  }
  return false;
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

//===----------------------------------------------------------------------===//
// Phase 3: Alloc/Dealloc-family instructions (9 instructions)
//===----------------------------------------------------------------------===//

// AllocStackInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAllocStackInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  auto hasDynamicLifetime = DoesNotHaveDynamicLifetime;
  auto isLexical = IsNotLexical;
  auto isFromVarDecl = IsNotFromVarDecl;
  UsesMoveableValueDebugInfo_t usesMoveableValueDebugInfo =
      DoesNotUseMoveableValueDebugInfo;

  StringRef attributeName;
  SourceLoc attributeLoc;
  while (parseSILOptional(attributeName, attributeLoc, P)) {
    if (attributeName == "dynamic_lifetime")
      hasDynamicLifetime = HasDynamicLifetime;
    else if (attributeName == "lexical")
      isLexical = IsLexical;
    else if (attributeName == "var_decl")
      isFromVarDecl = IsFromVarDecl;
    else if (attributeName == "moveable_value_debuginfo")
      usesMoveableValueDebugInfo = UsesMoveableValueDebugInfo;
    else {
      P.P.diagnose(attributeLoc, diag::sil_invalid_attribute_for_instruction,
                   attributeName, "alloc_stack");
      return true;
    }
  }

  SILType Ty;
  if (P.parseSILType(Ty))
    return true;

  SILDebugVariable VarInfo;
  if (P.parseSILDebugVar(VarInfo) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (Ty.isMoveOnly())
    usesMoveableValueDebugInfo = UsesMoveableValueDebugInfo;

  // It doesn't make sense to attach a debug var info if the name is empty
  if (VarInfo.Name.size())
    ResultVal = B.createAllocStack(InstLoc, Ty, VarInfo, hasDynamicLifetime,
                                   isLexical, isFromVarDecl,
                                   usesMoveableValueDebugInfo);
  else
    ResultVal = B.createAllocStack(InstLoc, Ty, {}, hasDynamicLifetime,
                                   isLexical, isFromVarDecl,
                                   usesMoveableValueDebugInfo);
  return false; // Success
}

// AllocPackInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAllocPackInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;

  if (P.parseSILType(Ty))
    return true;

  ResultVal = B.createAllocPack(InstLoc, Ty);
  return false; // Success
}

// AllocPackMetadataInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAllocPackMetadataInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;

  if (P.parseSILType(Ty))
    return true;

  ResultVal = B.createAllocPackMetadata(InstLoc, Ty);
  return false; // Success
}

// DeallocStackInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocStackInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocStack(InstLoc, Val);
  return false; // Success
}

// DeallocStackRefInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocStackRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocStackRef(InstLoc, Val);
  return false; // Success
}

// DeallocPackInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocPackInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocPack(InstLoc, Val);
  return false; // Success
}

// DeallocPackMetadataInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocPackMetadataInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocPackMetadata(InstLoc, Val);
  return false; // Success
}

// DeallocRefInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocRef(InstLoc, Val);
  return false; // Success
}

// DeallocPartialRefInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocPartialRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Metatype, Instance;

  if (P.parseTypedValueRef(Instance, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseTypedValueRef(Metatype, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocPartialRef(InstLoc, Instance, Metatype);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Phase 3 Batch 2: Complex Alloc/Dealloc Instructions
//===----------------------------------------------------------------------===//

// BeginDeallocRefInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitBeginDeallocRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val, allocation;

  if (P.parseTypedValueRef(Val, B) || P.parseVerbatim("of") ||
      P.parseTypedValueRef(allocation, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createBeginDeallocRef(InstLoc, Val, allocation);
  return false; // Success
}

// DeallocExistentialBoxInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocExistentialBoxInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  CanType ConcreteTy;

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
      P.parseASTType(ConcreteTy) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocExistentialBox(InstLoc, ConcreteTy, Val);
  return false; // Success
}

// AllocGlobalInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAllocGlobalInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  Identifier GlobalName;
  SourceLoc IdLoc;

  if (P.P.parseToken(tok::at_sign, diag::expected_sil_value_name) ||
      P.parseSILIdentifier(GlobalName, IdLoc,
                           diag::expected_sil_value_name) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  // Go through list of global variables in the SILModule.
  SILGlobalVariable *global = P.SILMod.lookUpGlobalVariable(GlobalName.str());
  if (!global) {
    P.P.diagnose(IdLoc, diag::sil_global_variable_not_found, GlobalName);
    return true;
  }

  ResultVal = B.createAllocGlobal(InstLoc, global);
  return false; // Success
}

// AllocExistentialBoxInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAllocExistentialBoxInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType ExistentialTy;
  CanType ConcreteFormalTy;
  SourceLoc TyLoc;

  if (P.parseSILType(ExistentialTy) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
      P.parseASTType(ConcreteFormalTy, TyLoc) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  // Collect conformances for the type.
  ArrayRef<ProtocolConformanceRef> conformances =
      collectExistentialConformances(P.P, ConcreteFormalTy, TyLoc,
                                     ExistentialTy.getASTType());

  ResultVal = B.createAllocExistentialBox(InstLoc, ExistentialTy,
                                          ConcreteFormalTy, conformances);
  return false; // Success
}

// AllocRefInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAllocRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool IsObjC = false;
  bool OnStack = false;
  bool isBare = false;
  SmallVector<SILType, 2> ElementTypes;
  SmallVector<SILValue, 2> ElementCounts;

  // Parse optional attributes: [objc], [stack], [bare], [tail_elems ...]
  while (P.P.consumeIf(tok::l_square)) {
    Identifier Id;
    SourceLoc IdLoc;
    if (P.parseSILIdentifier(Id, IdLoc, diag::expected_in_attribute_list))
      return true;

    StringRef Optional = Id.str();
    if (Optional == "objc") {
      IsObjC = true;
    } else if (Optional == "stack") {
      OnStack = true;
    } else if (Optional == "bare") {
      isBare = true;
    } else if (Optional == "tail_elems") {
      SILType ElemTy;
      if (P.parseSILType(ElemTy) || !P.P.Tok.isAnyOperator() ||
          P.P.Tok.getText() != "*")
        return true;
      P.P.consumeToken();

      SILValue ElemCount;
      if (P.parseTypedValueRef(ElemCount, B))
        return true;

      ElementTypes.push_back(ElemTy);
      ElementCounts.push_back(ElemCount);
    } else {
      return true;
    }

    if (P.P.parseToken(tok::r_square, diag::expected_in_attribute_list))
      return true;
  }

  // Parse the type
  SILType ObjectType;
  if (P.parseSILType(ObjectType))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createAllocRef(InstLoc, ObjectType, IsObjC, OnStack, isBare,
                               ElementTypes, ElementCounts);
  return false; // Success
}

// AllocRefDynamicInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAllocRefDynamicInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool IsObjC = false;
  bool OnStack = false;
  SmallVector<SILType, 2> ElementTypes;
  SmallVector<SILValue, 2> ElementCounts;

  // Parse optional attributes: [objc], [stack], [tail_elems ...]
  while (P.P.consumeIf(tok::l_square)) {
    Identifier Id;
    SourceLoc IdLoc;
    if (P.parseSILIdentifier(Id, IdLoc, diag::expected_in_attribute_list))
      return true;

    StringRef Optional = Id.str();
    if (Optional == "objc") {
      IsObjC = true;
    } else if (Optional == "stack") {
      OnStack = true;
    } else if (Optional == "tail_elems") {
      SILType ElemTy;
      if (P.parseSILType(ElemTy) || !P.P.Tok.isAnyOperator() ||
          P.P.Tok.getText() != "*")
        return true;
      P.P.consumeToken();

      SILValue ElemCount;
      if (P.parseTypedValueRef(ElemCount, B))
        return true;

      ElementTypes.push_back(ElemTy);
      ElementCounts.push_back(ElemCount);
    } else {
      return true;
    }

    if (P.P.parseToken(tok::r_square, diag::expected_in_attribute_list))
      return true;
  }

  // Parse the metadata operand (difference from AllocRefInst)
  SILValue Metadata;
  if (P.parseTypedValueRef(Metadata, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ","))
    return true;

  // Parse the type
  SILType ObjectType;
  if (P.parseSILType(ObjectType) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createAllocRefDynamic(InstLoc, Metadata, ObjectType, IsObjC,
                                      OnStack, ElementTypes, ElementCounts);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Phase 3 Batch 3: Struct-family Instructions
//===----------------------------------------------------------------------===//

// StructInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitStructInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;

  if (P.parseSILType(Ty) ||
      P.P.parseToken(tok::l_paren, diag::expected_tok_in_sil_instr, "("))
    return true;

  // Parse a list of SILValue
  SmallVector<SILValue, 8> OpList;
  if (P.P.Tok.isNot(tok::r_paren)) {
    do {
      SILValue Val;
      if (P.parseTypedValueRef(Val, B))
        return true;
      OpList.push_back(Val);
    } while (P.P.consumeIf(tok::comma));
  }

  if (P.P.parseToken(tok::r_paren, diag::expected_tok_in_sil_instr, ")"))
    return true;

  ValueOwnershipKind forwardingOwnership =
      P.F && P.F->hasOwnership() ? getSILValueOwnership(OpList, Ty)
                                 : ValueOwnershipKind(OwnershipKind::None);
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createStruct(InstLoc, Ty, OpList, forwardingOwnership);
  return false; // Success
}

// StructExtractInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitStructExtractInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  ValueDecl *FieldV;
  SourceLoc NameLoc = P.P.Tok.getLoc();

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILDottedPath(FieldV))
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!FieldV || !isa<VarDecl>(FieldV)) {
    P.P.diagnose(NameLoc, diag::sil_struct_inst_wrong_field);
    return true;
  }
  VarDecl *Field = cast<VarDecl>(FieldV);

  // FIXME: substitution means this type should be explicit to improve
  // performance.
  auto ResultTy = Val->getType().getFieldType(Field, P.SILMod,
                                              B.getTypeExpansionContext());
  ResultVal = B.createStructExtract(InstLoc, Val, Field,
                                    ResultTy.getObjectType(), forwardingOwnership);
  return false; // Success
}

// StructElementAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitStructElementAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  ValueDecl *FieldV;
  SourceLoc NameLoc = P.P.Tok.getLoc();

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILDottedPath(FieldV) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!FieldV || !isa<VarDecl>(FieldV)) {
    P.P.diagnose(NameLoc, diag::sil_struct_inst_wrong_field);
    return true;
  }
  VarDecl *Field = cast<VarDecl>(FieldV);

  // FIXME: substitution means this type should be explicit to improve
  // performance.
  auto ResultTy = Val->getType().getFieldType(Field, P.SILMod,
                                              B.getTypeExpansionContext());
  ResultVal = B.createStructElementAddr(InstLoc, Val, Field,
                                        ResultTy.getAddressType());
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Phase 3 Batch 3: Enum-family Instructions
//===----------------------------------------------------------------------===//

// EnumInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitEnumInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;
  SILDeclRef Elt;
  SILValue Operand;

  if (P.parseSILType(Ty) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILDeclRef(Elt))
    return true;

  if (P.P.Tok.is(tok::comma) && !P.peekSILDebugLocation()) {
    P.P.consumeToken(tok::comma);
    if (P.parseTypedValueRef(Operand, B))
      return true;
  }

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ValueOwnershipKind forwardingOwnership =
      Operand ? Operand->getOwnershipKind()
              : ValueOwnershipKind(OwnershipKind::None);

  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createEnum(InstLoc, Operand, cast<EnumElementDecl>(Elt.getDecl()),
                   Ty, forwardingOwnership);
  return false; // Success
}

// InitEnumDataAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitInitEnumDataAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Operand;
  SILDeclRef EltRef;

  if (P.parseTypedValueRef(Operand, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILDeclRef(EltRef) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  EnumElementDecl *Elt = cast<EnumElementDecl>(EltRef.getDecl());
  auto ResultTy = Operand->getType().getEnumElementType(
      Elt, P.SILMod, B.getTypeExpansionContext());

  ResultVal = B.createInitEnumDataAddr(InstLoc, Operand, Elt, ResultTy);
  return false; // Success
}

// UncheckedEnumDataInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitUncheckedEnumDataInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Operand;
  SILDeclRef EltRef;

  if (P.parseTypedValueRef(Operand, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILDeclRef(EltRef))
    return true;

  ValueOwnershipKind forwardingOwnership = Operand->getOwnershipKind();
  P.parseForwardingOwnershipKind(forwardingOwnership);

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  EnumElementDecl *Elt = cast<EnumElementDecl>(EltRef.getDecl());
  auto ResultTy = Operand->getType().getEnumElementType(
      Elt, P.SILMod, B.getTypeExpansionContext());

  ResultVal = B.createUncheckedEnumData(InstLoc, Operand, Elt, ResultTy,
                                        forwardingOwnership);
  return false; // Success
}

// UncheckedTakeEnumDataAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitUncheckedTakeEnumDataAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Operand;
  SILDeclRef EltRef;

  if (P.parseTypedValueRef(Operand, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILDeclRef(EltRef) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  EnumElementDecl *Elt = cast<EnumElementDecl>(EltRef.getDecl());
  auto ResultTy = Operand->getType().getEnumElementType(
      Elt, P.SILMod, B.getTypeExpansionContext());

  ResultVal =
      B.createUncheckedTakeEnumDataAddr(InstLoc, Operand, Elt, ResultTy);
  return false; // Success
}

// InjectEnumAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitInjectEnumAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Operand;
  SILDeclRef EltRef;

  if (P.parseTypedValueRef(Operand, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILDeclRef(EltRef) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  EnumElementDecl *Elt = cast<EnumElementDecl>(EltRef.getDecl());
  ResultVal = B.createInjectEnumAddr(InstLoc, Operand, Elt);
  return false; // Success
}

// SelectEnumInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitSelectEnumInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B))
    return true;

  SmallVector<std::pair<EnumElementDecl *, SILParser::UnresolvedValueName>, 4>
      CaseValueNames;
  std::optional<SILParser::UnresolvedValueName> DefaultValueName;
  while (P.P.consumeIf(tok::comma)) {
    Identifier BBName;
    SourceLoc BBLoc;
    SILParser::UnresolvedValueName tmp;

    // Parse 'default' sil-value.
    if (P.P.consumeIf(tok::kw_default)) {
      if (P.parseValueName(tmp))
        return true;
      DefaultValueName = tmp;
      break;
    }

    // Parse 'case' sil-decl-ref ':' sil-value.
    if (P.P.consumeIf(tok::kw_case)) {
      SILDeclRef ElemRef;
      if (P.parseSILDeclRef(ElemRef))
        return true;
      assert(ElemRef.hasDecl() && isa<EnumElementDecl>(ElemRef.getDecl()));
      P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":");
      P.parseValueName(tmp);
      CaseValueNames.push_back(
          std::make_pair(cast<EnumElementDecl>(ElemRef.getDecl()), tmp));
      continue;
    }

    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "case or default");
    return true;
  }

  // Parse the type of the result operands.
  SILType ResultType;
  if (P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":") ||
      P.parseSILType(ResultType))
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  // Resolve the results.
  SmallVector<std::pair<EnumElementDecl *, SILValue>, 4> CaseValues;
  SILValue DefaultValue;
  if (DefaultValueName)
    DefaultValue = P.getLocalValue(*DefaultValueName, ResultType, InstLoc, B);
  for (auto &caseName : CaseValueNames)
    CaseValues.push_back(std::make_pair(
        caseName.first,
        P.getLocalValue(caseName.second, ResultType, InstLoc, B)));

  ResultVal =
      B.createSelectEnum(InstLoc, Val, ResultType, DefaultValue,
                         CaseValues, std::nullopt, ProfileCounter());
  return false; // Success
}

// SelectEnumAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitSelectEnumAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B))
    return true;

  SmallVector<std::pair<EnumElementDecl *, SILParser::UnresolvedValueName>, 4>
      CaseValueNames;
  std::optional<SILParser::UnresolvedValueName> DefaultValueName;
  while (P.P.consumeIf(tok::comma)) {
    Identifier BBName;
    SourceLoc BBLoc;
    SILParser::UnresolvedValueName tmp;

    // Parse 'default' sil-value.
    if (P.P.consumeIf(tok::kw_default)) {
      if (P.parseValueName(tmp))
        return true;
      DefaultValueName = tmp;
      break;
    }

    // Parse 'case' sil-decl-ref ':' sil-value.
    if (P.P.consumeIf(tok::kw_case)) {
      SILDeclRef ElemRef;
      if (P.parseSILDeclRef(ElemRef))
        return true;
      assert(ElemRef.hasDecl() && isa<EnumElementDecl>(ElemRef.getDecl()));
      P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":");
      P.parseValueName(tmp);
      CaseValueNames.push_back(
          std::make_pair(cast<EnumElementDecl>(ElemRef.getDecl()), tmp));
      continue;
    }

    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "case or default");
    return true;
  }

  // Parse the type of the result operands.
  SILType ResultType;
  if (P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":") ||
      P.parseSILType(ResultType) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  // Resolve the results.
  SmallVector<std::pair<EnumElementDecl *, SILValue>, 4> CaseValues;
  SILValue DefaultValue;
  if (DefaultValueName)
    DefaultValue = P.getLocalValue(*DefaultValueName, ResultType, InstLoc, B);
  for (auto &caseName : CaseValueNames)
    CaseValues.push_back(std::make_pair(
        caseName.first,
        P.getLocalValue(caseName.second, ResultType, InstLoc, B)));

  ResultVal = B.createSelectEnumAddr(InstLoc, Val, ResultType,
                                     DefaultValue, CaseValues);
  return false; // Success
}

// SwitchEnumInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitSwitchEnumInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B))
    return true;

  SmallVector<std::pair<EnumElementDecl *, SILBasicBlock *>, 4> CaseBBs;
  SILBasicBlock *DefaultBB = nullptr;
  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  bool parsedComma = false;

  while (!P.peekSILDebugLocation() && P.P.consumeIf(tok::comma)) {
    parsedComma = true;

    Identifier BBName;
    SourceLoc BBLoc;
    // Parse 'case' sil-decl-ref ':' sil-identifier.
    if (P.P.consumeIf(tok::kw_case)) {
      parsedComma = false;
      if (DefaultBB) {
        P.P.diagnose(P.P.Tok, diag::case_after_default);
        return true;
      }
      SILDeclRef ElemRef;
      if (P.parseSILDeclRef(ElemRef))
        return true;
      assert(ElemRef.hasDecl() && isa<EnumElementDecl>(ElemRef.getDecl()));
      P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":");
      P.parseSILIdentifier(BBName, BBLoc, diag::expected_sil_block_name);
      CaseBBs.push_back({cast<EnumElementDecl>(ElemRef.getDecl()),
                         P.getBBForReference(BBName, BBLoc)});
      continue;
    }

    // Parse 'default' sil-identifier.
    if (P.P.consumeIf(tok::kw_default)) {
      parsedComma = false;
      P.parseSILIdentifier(BBName, BBLoc, diag::expected_sil_block_name);
      DefaultBB = P.getBBForReference(BBName, BBLoc);
      continue;
    }
    break;
  }

  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (parsedComma || (CaseBBs.empty() && !DefaultBB)) {
    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "case or default");
    return true;
  }

  ResultVal =
      B.createSwitchEnum(InstLoc, Val, DefaultBB, CaseBBs, std::nullopt,
                         ProfileCounter(), forwardingOwnership);
  return false; // Success
}

// SwitchEnumAddrInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitSwitchEnumAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B))
    return true;

  SmallVector<std::pair<EnumElementDecl *, SILBasicBlock *>, 4> CaseBBs;
  SILBasicBlock *DefaultBB = nullptr;
  bool parsedComma = false;

  while (!P.peekSILDebugLocation() && P.P.consumeIf(tok::comma)) {
    parsedComma = true;

    Identifier BBName;
    SourceLoc BBLoc;
    // Parse 'case' sil-decl-ref ':' sil-identifier.
    if (P.P.consumeIf(tok::kw_case)) {
      parsedComma = false;
      if (DefaultBB) {
        P.P.diagnose(P.P.Tok, diag::case_after_default);
        return true;
      }
      SILDeclRef ElemRef;
      if (P.parseSILDeclRef(ElemRef))
        return true;
      assert(ElemRef.hasDecl() && isa<EnumElementDecl>(ElemRef.getDecl()));
      P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":");
      P.parseSILIdentifier(BBName, BBLoc, diag::expected_sil_block_name);
      CaseBBs.push_back({cast<EnumElementDecl>(ElemRef.getDecl()),
                         P.getBBForReference(BBName, BBLoc)});
      continue;
    }

    // Parse 'default' sil-identifier.
    if (P.P.consumeIf(tok::kw_default)) {
      parsedComma = false;
      P.parseSILIdentifier(BBName, BBLoc, diag::expected_sil_block_name);
      DefaultBB = P.getBBForReference(BBName, BBLoc);
      continue;
    }
    break;
  }

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (parsedComma || (CaseBBs.empty() && !DefaultBB)) {
    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "case or default");
    return true;
  }

  ResultVal = B.createSwitchEnumAddr(InstLoc, Val, DefaultBB, CaseBBs);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Phase 4: Load/Store Basic Instructions
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

//===----------------------------------------------------------------------===//
// Phase 5: Copy/Destroy/Move Instructions
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
