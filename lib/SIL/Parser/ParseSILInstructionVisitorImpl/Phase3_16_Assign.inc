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

  if (P.parseAssignOrInitMode(mode, *this) ||
      P.parseAssignOrInitAssignments(assignments, *this) ||
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
