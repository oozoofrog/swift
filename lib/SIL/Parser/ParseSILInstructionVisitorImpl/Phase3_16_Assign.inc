//===--- Phase3_16_Assign.inc - Phase 3-16 Visit Methods -*- C++ -*-===//
//
// Phase 3-16: Pack index & Assign instructions (3 instructions)
//   - PackPackIndexInst
//   - AssignInst / AssignOrInitInst
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE316_ASSIGN_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE316_ASSIGN_INC

static bool parseAssignOrInitModeVisitor(SILParser &Parser,
                                         AssignOrInitInst::Mode &Result) {
  StringRef Str;
  SourceLoc Loc;
  if (!parseSILOptional(Str, Loc, Parser)) {
    Result = AssignOrInitInst::Unknown;
    return false;
  }

  auto Mode = llvm::StringSwitch<AssignOrInitInst::Mode>(Str)
                  .Case("init", AssignOrInitInst::Init)
                  .Case("set", AssignOrInitInst::Set)
                  .Default(AssignOrInitInst::Unknown);

  if (Mode == AssignOrInitInst::Unknown)
    return true;

  Result = Mode;
  return false;
}

static bool parseAssignOrInitAssignmentsVisitor(
    llvm::SmallVectorImpl<unsigned> &assignments, SILParser &Parser) {
  for (;;) {
    SourceLoc loc;
    if (!Parser.P.consumeIf(tok::l_square))
      return false;

    Identifier Id;
    if (Parser.parseSILIdentifier(Id, loc, diag::expected_in_attribute_list))
      return true;

    if (!Id.is("assign")) {
      Parser.P.diagnose(loc, diag::sil_invalid_attribute_for_expected,
                        Id.str(), "assign");
      return true;
    }

    if (!Parser.P.consumeIf(tok::equal)) {
      Parser.P.diagnose(loc, diag::expected_equal_in_sil_instr);
      return true;
    }

    uint64_t index;
    if (Parser.parseInteger(index, diag::expected_in_attribute_list))
      return true;

    if (Parser.P.parseToken(tok::r_square, diag::expected_in_attribute_list))
      return true;

    assignments.push_back(static_cast<unsigned>(index));
  }
}

bool SILInstructionParserVisitor::visitPackPackIndexInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  unsigned componentIndex = 0;
  CanPackType packType;
  SILValue indexValue;

  if (P.parseInteger(componentIndex, diag::expected_sil_constant) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseValueRef(indexValue, SILType::getPackIndexType(P.P.Context),
                      InstLoc, B) ||
      P.parseVerbatim("of") ||
      P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
      P.parseASTPackType(packType) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createPackPackIndex(InstLoc, componentIndex, indexValue,
                                    packType);
  return false; // Success
}

bool SILInstructionParserVisitor::visitAssignInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILParser::UnresolvedValueName fromName;
  Identifier toToken;
  SourceLoc toLoc;
  SILValue addrVal;
  SourceLoc addrLoc;
  std::optional<AssignOwnershipQualifier> assignQualifier;

  if (P.parseValueName(fromName) ||
      P.parseSILIdentifier(toToken, toLoc, diag::expected_tok_in_sil_instr,
                           "to"))
    return true;

  auto parseAssignOwnership = [](StringRef Str) {
    return llvm::StringSwitch<std::optional<AssignOwnershipQualifier>>(Str)
        .Case("reassign", AssignOwnershipQualifier::Reassign)
        .Case("reinit", AssignOwnershipQualifier::Reinit)
        .Case("init", AssignOwnershipQualifier::Init)
        .Default(std::nullopt);
  };
  if (P.parseSILQualifier<AssignOwnershipQualifier>(assignQualifier,
                                                    parseAssignOwnership))
    return true;

  if (P.parseTypedValueRef(addrVal, addrLoc, B) ||
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
  if (!assignQualifier)
    assignQualifier = AssignOwnershipQualifier::Unknown;

  SILValue rhsValue =
      P.getLocalValue(fromName, valueTy, InstLoc, B);
  ResultVal = B.createAssign(InstLoc, rhsValue, addrVal,
                             assignQualifier.value());
  return false; // Success
}

bool SILInstructionParserVisitor::visitAssignOrInitInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  ValueDecl *propDecl;
  SILValue selfValue, srcValue, initFn, setFn;
  AssignOrInitInst::Mode mode;
  llvm::SmallVector<unsigned, 2> assignments;

  if (parseAssignOrInitModeVisitor(P, mode) ||
      parseAssignOrInitAssignmentsVisitor(assignments, P) ||
      P.parseSILDottedPath(propDecl) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseVerbatim("self") || P.parseTypedValueRef(selfValue, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseVerbatim("value") || P.parseTypedValueRef(srcValue, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseVerbatim("init") || P.parseTypedValueRef(initFn, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseVerbatim("set") || P.parseTypedValueRef(setFn, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto *AI = B.createAssignOrInit(InstLoc, cast<VarDecl>(propDecl), selfValue,
                                  srcValue, initFn, setFn, mode);
  for (unsigned index : assignments)
    AI->markAsInitialized(index);

  ResultVal = AI;
  return false; // Success
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE316_ASSIGN_INC
