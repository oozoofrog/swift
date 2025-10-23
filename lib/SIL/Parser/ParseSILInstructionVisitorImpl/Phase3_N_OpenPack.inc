//===--- Phase3_N_OpenPack.inc - Phase 3-N Open/Pack Methods -*- C++ -*-===//
//
// Phase 3-N (part 2): Open existential / pack / access / cast instructions
//   - OpenExistential*, TypeValue, Pack helpers
//   - Checked cast utilities
//   - MarkUninitialized / MarkFunctionEscape
//   - Access instructions (begin/end access variants)
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE3N_OPENPACK_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE3N_OPENPACK_INC

SubstitutionMap getApplySubstitutionsFromParsed(
    SILParser &SP, GenericSignature genericSig,
    ArrayRef<ParsedSubstitution> parsedSubs);

//===----------------------------------------------------------------------===//
// Open existential instructions
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitOpenExistentialBoxInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;

  if (P.parseTypedValueRef(Val, B) || P.parseVerbatim("to") ||
      P.parseSILType(Ty) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createOpenExistentialBox(InstLoc, Val, Ty);
  return false; // Success
}

bool SILInstructionParserVisitor::visitOpenExistentialBoxValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;

  if (P.parseTypedValueRef(Val, B) || P.parseVerbatim("to") ||
      P.parseSILType(Ty))
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createOpenExistentialBoxValue(InstLoc, Val, Ty,
                                              forwardingOwnership);
  return false; // Success
}

bool SILInstructionParserVisitor::visitOpenExistentialMetatypeInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;

  if (P.parseTypedValueRef(Val, B) || P.parseVerbatim("to") ||
      P.parseSILType(Ty) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createOpenExistentialMetatype(InstLoc, Val, Ty);
  return false; // Success
}

bool SILInstructionParserVisitor::visitOpenExistentialRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;

  if (P.parseTypedValueRef(Val, B) || P.parseVerbatim("to") ||
      P.parseSILType(Ty))
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createOpenExistentialRef(InstLoc, Val, Ty,
                                         forwardingOwnership);
  return false; // Success
}

bool SILInstructionParserVisitor::visitOpenExistentialValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType Ty;

  if (P.parseTypedValueRef(Val, B) || P.parseVerbatim("to") ||
      P.parseSILType(Ty))
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createOpenExistentialValue(InstLoc, Val, Ty,
                                           forwardingOwnership);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Type / Pack utilities
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitTypeValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;
  CanType paramType;

  if (P.parseSILType(Ty) || P.parseVerbatim("for") ||
      P.parseASTTypeOrValue(paramType))
    return true;

  ResultVal = B.createTypeValue(InstLoc, Ty, paramType);
  return false; // Success
}

bool SILInstructionParserVisitor::visitPackLengthInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  CanPackType packType;

  if (P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
      P.parseASTPackType(packType))
    return true;

  ResultVal = B.createPackLength(InstLoc, packType);
  return false; // Success
}

bool SILInstructionParserVisitor::visitScalarPackIndexInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  unsigned componentIndex = 0;
  CanPackType packType;

  if (P.parseInteger(componentIndex, diag::expected_sil_constant) ||
      P.parseVerbatim("of") ||
      P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
      P.parseASTPackType(packType))
    return true;

  ResultVal = B.createScalarPackIndex(InstLoc, componentIndex, packType);
  return false; // Success
}

bool SILInstructionParserVisitor::visitOpenPackElementInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseValueRef(Val, SILType::getPackIndexType(P.P.Context), InstLoc, B) ||
      P.parseVerbatim("of"))
    return true;

  GenericParamList *openedGenerics = nullptr;
  if (!P.P.startsWithLess(P.P.Tok)) {
    P.P.diagnose(P.P.Tok, diag::expected_generic_signature);
    return true;
  }
  openedGenerics = P.maybeParseGenericParams().getPtrOrNull();
  if (!openedGenerics)
    return true;

  auto openedGenericsSig = handleSILGenericParams(openedGenerics, &P.P.SF);
  if (!openedGenericsSig)
    return true;

  SubstitutionMap openedSubMap;
  if (P.parseVerbatim("at"))
    return true;
  SmallVector<ParsedSubstitution> parsedOpenedSubs;
  if (P.parseSubstitutions(parsedOpenedSubs))
    return true;
  openedSubMap = getApplySubstitutionsFromParsed(P, openedGenericsSig,
                                                 parsedOpenedSubs);
  if (!openedSubMap)
    return true;

  CanType shapeClass;
  SourceLoc shapeClassLoc;
  if (!P.P.consumeIf(tok::comma) || P.parseVerbatim("shape") ||
      P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
      P.parseASTType(shapeClass, shapeClassLoc, openedGenericsSig,
                     openedGenerics, /*wantContextualType*/ true))
    return true;

  shapeClass = shapeClass->mapTypeOutOfContext()->getCanonicalType();
  auto shapeParam = dyn_cast<GenericTypeParamType>(shapeClass);
  if (!shapeParam || !shapeParam->isParameterPack()) {
    P.P.diagnose(shapeClassLoc, diag::opened_shape_class_not_pack_param);
    return true;
  }

  UUID uuid;
  if (!P.P.consumeIf(tok::comma) || P.parseVerbatim("uuid") ||
      P.parseUUIDString(uuid, diag::sil_expected_uuid))
    return true;

  auto openedElementSig = P.P.Context.getOpenedElementSignature(
      openedGenericsSig.getCanonicalSignature(), shapeParam);

  auto openedEnv = GenericEnvironment::forOpenedElement(
      openedElementSig, uuid, shapeParam, openedSubMap);

  ResultVal = B.createOpenPackElement(InstLoc, Val, openedEnv);

  auto &entry = P.OpenedPackElements[uuid];
  if (entry.DefinitionPoint.isValid()) {
    P.P.diagnose(OpcodeLoc, diag::multiple_open_pack_element);
    P.P.diagnose(entry.DefinitionPoint, diag::sil_previous_instruction);
  } else {
    entry.DefinitionPoint = OpcodeLoc;
    entry.Params = openedGenerics;
    entry.Environment = openedEnv;
  }
  return false; // Success
}

bool SILInstructionParserVisitor::visitPackElementGetInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue index, pack;
  SILType elementType;

  if (P.parseValueRef(index, SILType::getPackIndexType(P.P.Context), InstLoc, B) ||
      P.parseVerbatim("of") ||
      P.parseTypedValueRef(pack, B) ||
      P.parseVerbatim("as") ||
      P.parseSILType(elementType))
    return true;

  ResultVal = B.createPackElementGet(InstLoc, index, pack, elementType);
  return false; // Success
}

bool SILInstructionParserVisitor::visitPackElementSetInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue value, index, pack;

  if (P.parseTypedValueRef(value, B) || P.parseVerbatim("into") ||
      P.parseValueRef(index, SILType::getPackIndexType(P.P.Context), InstLoc, B) ||
      P.parseVerbatim("of") ||
      P.parseTypedValueRef(pack, B))
    return true;

  ResultVal = B.createPackElementSet(InstLoc, value, index, pack);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Checked cast instructions
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitUncheckedRefCastAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  CanType SourceType, TargetType;
  SILValue SourceAddr, DestAddr;

  auto parseSourceAndDestAddress = [&]() {
    return P.parseFormalTypeAndValue(SourceType, SourceAddr) ||
           P.parseVerbatim("to") ||
           P.parseFormalTypeAndValue(TargetType, DestAddr);
  };

  if (parseSourceAndDestAddress() || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createUncheckedRefCastAddr(InstLoc, SourceAddr, SourceType,
                                   DestAddr, TargetType);
  return false; // Success
}

bool SILInstructionParserVisitor::visitUnconditionalCheckedCastAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  CheckedCastInstOptions options =
      P.parseCheckedCastInstOptions(nullptr);
  CanType SourceType, TargetType;
  SILValue SourceAddr, DestAddr;

  auto parseSourceAndDestAddress = [&]() {
    return P.parseFormalTypeAndValue(SourceType, SourceAddr) ||
           P.parseVerbatim("to") ||
           P.parseFormalTypeAndValue(TargetType, DestAddr);
  };

  if (parseSourceAndDestAddress() || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createUnconditionalCheckedCastAddr(
      InstLoc, options, SourceAddr, SourceType, DestAddr, TargetType);
  return false; // Success
}

bool SILInstructionParserVisitor::visitUnconditionalCheckedCastInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  CheckedCastInstOptions options =
      P.parseCheckedCastInstOptions(nullptr);
  SILValue Val;
  CanType TargetType;

  if (P.parseTypedValueRef(Val, B) || P.parseVerbatim("to") ||
      P.parseASTType(TargetType))
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto opaque = Lowering::AbstractionPattern::getOpaque();
  ResultVal = B.createUnconditionalCheckedCast(
      InstLoc, options, Val,
      B.getFunction()->getLoweredType(opaque, TargetType),
      TargetType, forwardingOwnership);
  return false; // Success
}

bool SILInstructionParserVisitor::visitCheckedCastBranchInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool isExact = false;
  CheckedCastInstOptions options =
      P.parseCheckedCastInstOptions(&isExact);
  CanType SourceType, TargetType;
  SILValue Val;
  Identifier SuccessBBName, FailureBBName;
  SourceLoc SuccessBBLoc, FailureBBLoc;

  auto parseConditionalBranchDestinations = [&]() {
    return P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
           P.parseSILIdentifier(SuccessBBName, SuccessBBLoc,
                                diag::expected_sil_block_name) ||
           P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
           P.parseSILIdentifier(FailureBBName, FailureBBLoc,
                                diag::expected_sil_block_name);
  };

  if (P.parseASTType(SourceType) || P.parseVerbatim("in") ||
      P.parseTypedValueRef(Val, B) || P.parseVerbatim("to") ||
      P.parseASTType(TargetType) ||
      parseConditionalBranchDestinations())
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto opaque = Lowering::AbstractionPattern::getOpaque();
  auto *F = B.getFunction();
  ResultVal = B.createCheckedCastBranch(
      InstLoc, isExact, options, Val, SourceType,
      F->getLoweredType(opaque, TargetType), TargetType,
      P.getBBForReference(SuccessBBName, SuccessBBLoc),
      P.getBBForReference(FailureBBName, FailureBBLoc),
      forwardingOwnership);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Mark instructions
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitMarkUninitializedInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  if (P.P.parseToken(tok::l_square, diag::expected_tok_in_sil_instr, "["))
    return true;

  Identifier KindId;
  SourceLoc KindLoc = P.P.Tok.getLoc();
  if (P.P.consumeIf(tok::kw_var))
    KindId = P.P.Context.getIdentifier("var");
  else if (P.parseIdentifier(KindId, KindLoc,
                             /*diagnoseDollarPrefix*/ false,
                             diag::expected_tok_in_sil_instr, "kind"))
    return true;

  if (P.P.parseToken(tok::r_square, diag::expected_tok_in_sil_instr, "]"))
    return true;

  MarkUninitializedInst::Kind Kind;
  auto kindStr = KindId.str();
  if (kindStr == "var")
    Kind = MarkUninitializedInst::Var;
  else if (kindStr == "rootself")
    Kind = MarkUninitializedInst::RootSelf;
  else if (kindStr == "crossmodulerootself")
    Kind = MarkUninitializedInst::CrossModuleRootSelf;
  else if (kindStr == "derivedself")
    Kind = MarkUninitializedInst::DerivedSelf;
  else if (kindStr == "derivedselfonly")
    Kind = MarkUninitializedInst::DerivedSelfOnly;
  else if (kindStr == "delegatingself")
    Kind = MarkUninitializedInst::DelegatingSelf;
  else if (kindStr == "delegatingselfallocated")
    Kind = MarkUninitializedInst::DelegatingSelfAllocated;
  else if (kindStr == "out")
    Kind = MarkUninitializedInst::Out;
  else {
    P.P.diagnose(KindLoc, diag::expected_tok_in_sil_instr,
                 "var, rootself, crossmodulerootself, derivedself, "
                 "derivedselfonly, delegatingself, or delegatingselfallocated");
    return true;
  }

  SILValue Val;
  if (P.parseTypedValueRef(Val, B))
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createMarkUninitialized(InstLoc, Val, Kind, forwardingOwnership);
  return false; // Success
}

bool SILInstructionParserVisitor::visitMarkFunctionEscapeInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SmallVector<SILValue, 4> operands;

  do {
    SILValue Val;
    if (P.parseTypedValueRef(Val, B))
      return true;
    operands.push_back(Val);
  } while (!P.peekSILDebugLocation() && P.P.consumeIf(tok::comma));

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createMarkFunctionEscape(InstLoc, operands);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Access instructions
//===----------------------------------------------------------------------===//

static bool parseAccessInstruction(SILInstructionParserVisitor &V,
                                   SILInstructionKind opcode) {
  SILParser &P = V.P;
  SILBuilder &B = V.B;
  SILLocation InstLoc = RegularLocation(V.OpcodeLoc, /*implicit*/ false);

  ParsedEnum<SILAccessKind> kind;
  ParsedEnum<SILAccessEnforcement> enforcement;
  ParsedEnum<bool> aborting;
  ParsedEnum<bool> noNestedConflict;
  ParsedEnum<bool> fromBuiltin;

  bool isBeginAccess = (opcode == SILInstructionKind::BeginAccessInst ||
                        opcode == SILInstructionKind::BeginUnpairedAccessInst);
  bool wantsEnforcement =
      (isBeginAccess || opcode == SILInstructionKind::EndUnpairedAccessInst);

  while (P.P.consumeIf(tok::l_square)) {
    Identifier ident;
    SourceLoc identLoc;
    if (P.parseSILIdentifier(ident, identLoc,
                             diag::expected_in_attribute_list)) {
      if (P.P.consumeIf(tok::r_square))
        continue;
      return true;
    }
    StringRef attr = ident.str();

    auto setEnforcement = [&](SILAccessEnforcement value) {
      P.maybeSetEnum(wantsEnforcement, enforcement, value, attr, identLoc);
    };
    auto setKind = [&](SILAccessKind value) {
      P.maybeSetEnum(isBeginAccess, kind, value, attr, identLoc);
    };
    auto setAborting = [&](bool value) {
      P.maybeSetEnum(!isBeginAccess, aborting, value, attr, identLoc);
    };
    auto setNoNestedConflict = [&](bool value) {
      P.maybeSetEnum(isBeginAccess, noNestedConflict, value, attr, identLoc);
    };
    auto setFromBuiltin = [&](bool value) {
      P.maybeSetEnum(opcode != SILInstructionKind::EndAccessInst, fromBuiltin,
                     value, attr, identLoc);
    };

    if (attr == "unknown") {
      setEnforcement(SILAccessEnforcement::Unknown);
    } else if (attr == "static") {
      setEnforcement(SILAccessEnforcement::Static);
    } else if (attr == "dynamic") {
      setEnforcement(SILAccessEnforcement::Dynamic);
    } else if (attr == "unsafe") {
      setEnforcement(SILAccessEnforcement::Unsafe);
    } else if (attr == "signed") {
      setEnforcement(SILAccessEnforcement::Signed);
    } else if (attr == "init") {
      setKind(SILAccessKind::Init);
    } else if (attr == "read") {
      setKind(SILAccessKind::Read);
    } else if (attr == "modify") {
      setKind(SILAccessKind::Modify);
    } else if (attr == "deinit") {
      setKind(SILAccessKind::Deinit);
    } else if (attr == "abort") {
      setAborting(true);
    } else if (attr == "no_nested_conflict") {
      setNoNestedConflict(true);
    } else if (attr == "builtin") {
      setFromBuiltin(true);
    } else {
      P.P.diagnose(identLoc, diag::unknown_attr_name, attr);
    }

    if (!P.P.consumeIf(tok::r_square))
      return true;
  }

  if (isBeginAccess && !kind.isSet()) {
    P.P.diagnose(V.OpcodeLoc, diag::sil_expected_access_kind, V.OpcodeName);
    kind.Value = SILAccessKind::Read;
  }

  if (wantsEnforcement && !enforcement.isSet()) {
    P.P.diagnose(V.OpcodeLoc, diag::sil_expected_access_enforcement,
                 V.OpcodeName);
    enforcement.Value = SILAccessEnforcement::Unsafe;
  }

  if (!isBeginAccess && !aborting.isSet())
    aborting.Value = false;

  if (isBeginAccess && !noNestedConflict.isSet())
    noNestedConflict.Value = false;

  if (!fromBuiltin.isSet())
    fromBuiltin.Value = false;

  SILValue addrVal;
  SourceLoc addrLoc;
  if (P.parseTypedValueRef(addrVal, addrLoc, B))
    return true;

  SILValue bufferVal;
  SourceLoc bufferLoc;
  if (opcode == SILInstructionKind::BeginUnpairedAccessInst &&
      (P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
       P.parseTypedValueRef(bufferVal, bufferLoc, B)))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!addrVal->getType().isAddress()) {
    P.P.diagnose(addrLoc, diag::sil_operand_not_address,
                 "operand", V.OpcodeName);
    return true;
  }

  switch (opcode) {
  case SILInstructionKind::BeginAccessInst:
    V.ResultVal = B.createBeginAccess(InstLoc, addrVal, *kind, *enforcement,
                                      *noNestedConflict, *fromBuiltin);
    break;
  case SILInstructionKind::BeginUnpairedAccessInst:
    V.ResultVal = B.createBeginUnpairedAccess(InstLoc, addrVal, bufferVal,
                                              *kind, *enforcement,
                                              *noNestedConflict,
                                              *fromBuiltin);
    break;
  case SILInstructionKind::EndAccessInst:
    V.ResultVal = B.createEndAccess(InstLoc, addrVal, *aborting);
    break;
  case SILInstructionKind::EndUnpairedAccessInst:
    V.ResultVal = B.createEndUnpairedAccess(InstLoc, addrVal, *enforcement,
                                            *aborting, *fromBuiltin);
    break;
  default:
    llvm_unreachable("unexpected access instruction");
  }
  return false; // Success
}

bool SILInstructionParserVisitor::visitBeginAccessInst() {
  return parseAccessInstruction(*this, SILInstructionKind::BeginAccessInst);
}

bool SILInstructionParserVisitor::visitBeginUnpairedAccessInst() {
  return parseAccessInstruction(*this,
                                SILInstructionKind::BeginUnpairedAccessInst);
}

bool SILInstructionParserVisitor::visitEndAccessInst() {
  return parseAccessInstruction(*this, SILInstructionKind::EndAccessInst);
}

bool SILInstructionParserVisitor::visitEndUnpairedAccessInst() {
  return parseAccessInstruction(*this,
                                SILInstructionKind::EndUnpairedAccessInst);
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE3N_OPENPACK_INC
