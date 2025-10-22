//===--- Phase3_11_Memory.inc - Phase 3-11 Memory Instructions -*- C++ -*-===//
//
// Phase 3-11: Reference address, index, and global utility instructions
//
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE311_MEMORY_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE311_MEMORY_INC

bool SILInstructionParserVisitor::visitRefElementAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  ValueDecl *FieldV;
  SourceLoc NameLoc = P.P.Tok.getLoc();
  bool IsImmutable = false;

  if (parseSILOptional(IsImmutable, P, "immutable") ||
      P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILDottedPath(FieldV) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!FieldV || !isa<VarDecl>(FieldV)) {
    P.P.diagnose(NameLoc, diag::sil_ref_inst_wrong_field);
    return true;
  }

  auto *Field = cast<VarDecl>(FieldV);
  auto ResultTy =
      Val->getType().getFieldType(Field, P.SILMod, B.getTypeExpansionContext());

  ResultVal = B.createRefElementAddr(InstLoc, Val, Field, ResultTy,
                                     IsImmutable);
  return false; // Success
}

bool SILInstructionParserVisitor::visitRefTailAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType ResultObjTy;
  bool IsImmutable = false;

  if (parseSILOptional(IsImmutable, P, "immutable") ||
      P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILType(ResultObjTy) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createRefTailAddr(InstLoc, Val,
                                  ResultObjTy.getAddressType(), IsImmutable);
  return false; // Success
}

bool SILInstructionParserVisitor::visitIndexAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Base;
  SILValue IndexVal;
  bool needsStackProtection = false;

  if (parseSILOptional(needsStackProtection, P, "stack_protection") ||
      P.parseTypedValueRef(Base, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseTypedValueRef(IndexVal, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createIndexAddr(InstLoc, Base, IndexVal, needsStackProtection);
  return false; // Success
}

bool SILInstructionParserVisitor::visitVectorBaseAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Base;

  if (P.parseTypedValueRef(Base, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createVectorBaseAddr(InstLoc, Base);
  return false; // Success
}

bool SILInstructionParserVisitor::visitTailAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Base;
  SILValue IndexVal;
  SILType ResultObjTy;

  if (P.parseTypedValueRef(Base, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseTypedValueRef(IndexVal, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILType(ResultObjTy) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createTailAddr(InstLoc, Base, IndexVal,
                               ResultObjTy.getAddressType());
  return false; // Success
}

bool SILInstructionParserVisitor::visitIndexRawPointerInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Base;
  SILValue IndexVal;

  if (P.parseTypedValueRef(Base, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseTypedValueRef(IndexVal, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createIndexRawPointer(InstLoc, Base, IndexVal);
  return false; // Success
}

bool SILInstructionParserVisitor::visitVectorInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  if (P.P.parseToken(tok::l_paren, diag::expected_tok_in_sil_instr, "("))
    return true;

  SmallVector<SILValue, 4> Operands;
  if (P.P.Tok.isNot(tok::r_paren)) {
    do {
      SILValue Elem;
      if (P.parseTypedValueRef(Elem, B))
        return true;
      Operands.push_back(Elem);
    } while (P.P.consumeIf(tok::comma));
  }

  if (P.P.parseToken(tok::r_paren, diag::expected_tok_in_sil_instr, ")") ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createVector(InstLoc, Operands);
  return false; // Success
}

bool SILInstructionParserVisitor::visitObjectInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType ObjTy;

  if (P.parseSILType(ObjTy) ||
      P.P.parseToken(tok::l_paren, diag::expected_tok_in_sil_instr, "("))
    return true;

  SmallVector<SILValue, 4> Operands;
  bool opsAreTailElems = false;
  unsigned numBaseElems = 0;
  if (P.P.Tok.isNot(tok::r_paren)) {
    do {
      if (parseSILOptional(opsAreTailElems, P, "tail_elems"))
        return true;
      SILValue Elem;
      if (P.parseTypedValueRef(Elem, B))
        return true;
      Operands.push_back(Elem);
      if (!opsAreTailElems)
        numBaseElems = Operands.size();
    } while (P.P.consumeIf(tok::comma));
  }

  if (P.P.parseToken(tok::r_paren, diag::expected_tok_in_sil_instr, ")") ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createObject(InstLoc, ObjTy, Operands, numBaseElems);
  return false; // Success
}

bool SILInstructionParserVisitor::visitObjCProtocolInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  Identifier ProtocolName;
  SILType Ty;

  if (P.P.parseToken(tok::pound, diag::expected_sil_constant) ||
      P.parseSILIdentifier(ProtocolName, diag::expected_sil_constant) ||
      P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":") ||
      P.parseSILType(Ty) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  SmallVector<ValueDecl *, 4> results;
  P.SILMod.getSwiftModule()->lookupValue(
      ProtocolName, NLKind::UnqualifiedLookup, results);
  assert(results.size() == 1 && "protocol lookup should be unique");

  ResultVal =
      B.createObjCProtocol(InstLoc, cast<ProtocolDecl>(results[0]), Ty);
  return false; // Success
}

bool SILInstructionParserVisitor::visitGlobalAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  Identifier GlobalName;
  SourceLoc IdLoc;
  SILType Ty;

  if (P.P.parseToken(tok::at_sign, diag::expected_sil_value_name) ||
      P.parseSILIdentifier(GlobalName, IdLoc, diag::expected_sil_value_name) ||
      P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":") ||
      P.parseSILType(Ty))
    return true;

  auto *global = P.SILMod.lookUpGlobalVariable(GlobalName.str());
  if (!global) {
    P.P.diagnose(IdLoc, diag::sil_global_variable_not_found, GlobalName);
    return true;
  }

  SILValue dependencyToken;
  if (P.P.Tok.isContextualKeyword("depends_on")) {
    P.P.consumeToken();
    if (P.parseValueRef(dependencyToken,
                        SILType::getSILTokenType(P.P.Context),
                        RegularLocation(P.P.Tok.getLoc()), B))
      return true;
  }

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  SILType expectedType = global->getLoweredType().getAddressType();
  if (expectedType != Ty) {
    P.P.diagnose(IdLoc, diag::sil_value_use_type_mismatch, GlobalName.str(),
                 global->getLoweredType().getRawASTType(),
                 Ty.getRawASTType());
    return true;
  }

  ResultVal = B.createGlobalAddr(InstLoc, global, dependencyToken);
  return false; // Success
}

bool SILInstructionParserVisitor::visitGlobalValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  Identifier GlobalName;
  SourceLoc IdLoc;
  SILType Ty;
  bool isBare = false;

  if (P.P.consumeIf(tok::l_square)) {
    Identifier Attr;
    if (P.parseSILIdentifier(Attr, diag::expected_in_attribute_list) ||
        P.P.parseToken(tok::r_square, diag::expected_in_attribute_list))
      return true;
    if (Attr.str() == "bare")
      isBare = true;
    else
      return true;
  }

  if (P.P.parseToken(tok::at_sign, diag::expected_sil_value_name) ||
      P.parseSILIdentifier(GlobalName, IdLoc, diag::expected_sil_value_name) ||
      P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":") ||
      P.parseSILType(Ty))
    return true;

  auto *global = P.SILMod.lookUpGlobalVariable(GlobalName.str());
  if (!global) {
    P.P.diagnose(IdLoc, diag::sil_global_variable_not_found, GlobalName);
    return true;
  }

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  SILType expectedType = global->getLoweredType();
  if (expectedType != Ty) {
    P.P.diagnose(IdLoc, diag::sil_value_use_type_mismatch, GlobalName.str(),
                 global->getLoweredType().getRawASTType(),
                 Ty.getRawASTType());
    return true;
  }

  ResultVal = B.createGlobalValue(InstLoc, global, isBare);
  return false; // Success
}

bool SILInstructionParserVisitor::visitBaseAddrForOffsetInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;

  if (P.parseSILType(Ty) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createBaseAddrForOffset(InstLoc, Ty);
  return false; // Success
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE311_MEMORY_INC
