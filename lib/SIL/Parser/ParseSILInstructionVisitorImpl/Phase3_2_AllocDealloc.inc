//===--- Phase3_2_AllocDealloc.inc - Phase 3-2 Visit Methods --------*- C++ -*-===//
//
// Phase 3-2: Alloc/Dealloc Batch 2 - Complex instructions (6 instructions)
//
//===----------------------------------------------------------------------===//

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

