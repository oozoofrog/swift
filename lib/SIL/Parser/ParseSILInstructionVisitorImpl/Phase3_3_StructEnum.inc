//===--- Phase3_3_StructEnum.inc - Phase 3-3 Visit Methods ----------*- C++ -*-===//
//
// Phase 3-3: Struct/Enum families (12 instructions total)
//   - Struct family (3)
//   - Enum family (9)
//
//===----------------------------------------------------------------------===//

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


