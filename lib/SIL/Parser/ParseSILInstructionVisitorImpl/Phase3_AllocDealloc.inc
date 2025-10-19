//===--- Phase3_AllocDealloc.inc - Phase 3 Visit Methods ---------*- C++ -*-===//
//
// Phase 3: Alloc/Dealloc/Struct/Enum (27 instructions)
//   - Alloc/Dealloc Batch 1 (9): AllocStack, AllocPack, etc.
//   - Alloc/Dealloc Batch 2 (6): AllocRef, BeginDeallocRef, etc.
//   - Struct family (3): Struct, StructExtract, StructElementAddr
//   - Enum family (9): Enum, InitEnumDataAddr, etc.
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE3_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE3_INC

// Phase 3-1: Alloc/Dealloc Batch 1 - Simple instructions (9 instructions)
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
// Phase 3-2: Alloc/Dealloc Batch 2 - Complex Alloc/Dealloc Instructions
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
// Phase 3-3: Struct-family Instructions
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
// Phase 3-3: Enum-family Instructions
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


#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE3_INC
