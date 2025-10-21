//===--- Phase3_10_Existential.inc - Phase 3-10 Visit Methods -*- C++ -*-===//
//
// Phase 3-10: Existential / Advanced instructions (18 instructions)
//   - Existential init/deinit
//   - Dynamic method & block storage helpers
//   - Differentiable/Linear function utilities
//   - Async continuation helpers and symbol queries
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE310_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE310_INC

SubstitutionMap getApplySubstitutionsFromParsed(
    SILParser &SP, GenericSignature genericSig,
    ArrayRef<ParsedSubstitution> parsedSubs);

namespace {

static bool parseIndexListVisitor(Parser &P, StringRef label,
                                  SmallVectorImpl<unsigned> &indices,
                                  DiagRef parseIndexDiag) {
  if (P.parseToken(tok::l_square, diag::sil_autodiff_expected_lsquare,
                   "index list") ||
      P.parseSpecificIdentifier(
          label, diag::sil_autodiff_expected_index_list_label, label))
    return true;

  SourceLoc loc;
  while (P.Tok.is(tok::integer_literal)) {
    unsigned index;
    if (P.parseUnsignedInteger(index, loc, parseIndexDiag))
      return true;
    indices.push_back(index);
  }

  if (P.parseToken(tok::r_square, diag::sil_autodiff_expected_rsquare,
                   "index list"))
    return true;

  return false;
}

static bool parseSILDifferentiabilityWitnessConfigAndFunctionVisitor(
    Parser &P, SILParser &SP, SILLocation L,
    DifferentiabilityKind &resultDiffKind, AutoDiffConfig &resultConfig,
    SILFunction *&resultOrigFn) {
  if (P.parseToken(tok::l_square, diag::sil_autodiff_expected_lsquare,
                   "differentiability kind"))
    return true;

  resultDiffKind = llvm::StringSwitch<DifferentiabilityKind>(P.Tok.getText())
      .Case("forward", DifferentiabilityKind::Forward)
      .Case("reverse", DifferentiabilityKind::Reverse)
      .Case("normal", DifferentiabilityKind::Normal)
      .Case("linear", DifferentiabilityKind::Linear)
      .Default(DifferentiabilityKind::NonDifferentiable);

  if (resultDiffKind == DifferentiabilityKind::NonDifferentiable) {
    P.diagnose(P.Tok, diag::sil_diff_witness_unknown_kind, P.Tok.getText());
    return true;
  }

  P.consumeToken(tok::identifier);
  if (P.parseToken(tok::r_square, diag::sil_autodiff_expected_rsquare,
                   "differentiability kind"))
    return true;

  SmallVector<unsigned, 8> rawParameterIndices;
  SmallVector<unsigned, 8> rawResultIndices;
  if (parseIndexListVisitor(P, "parameters", rawParameterIndices,
                            diag::sil_autodiff_expected_parameter_index))
    return true;
  if (parseIndexListVisitor(P, "results", rawResultIndices,
                            diag::sil_autodiff_expected_result_index))
    return true;

  GenericSignature witnessGenSig = GenericSignature();
  SourceLoc witnessGenSigStartLoc = P.getEndOfPreviousLoc();
  if (auto *genericParams = P.maybeParseGenericParams().getPtrOrNull())
    witnessGenSig = handleSILGenericParams(genericParams, &P.SF);

  if (SP.parseSILFunctionRef(L, resultOrigFn))
    return true;

  if (witnessGenSig) {
    auto origGenSig =
        resultOrigFn->getLoweredFunctionType()->getSubstGenericSignature();

    auto areGenericParametersConsistent = [&]() -> bool {
      llvm::SmallDenseSet<GenericParamKey, 4> genericParamKeys;
      for (auto origGP : origGenSig.getGenericParams())
        genericParamKeys.insert(GenericParamKey(origGP.getPointer()));
      for (auto *witnessGP : witnessGenSig.getGenericParams())
        if (!genericParamKeys.erase(GenericParamKey(witnessGP)))
          return false;
      return genericParamKeys.empty();
    };

    if (!areGenericParametersConsistent()) {
      P.diagnose(witnessGenSigStartLoc,
                 diag::sil_diff_witness_invalid_generic_signature,
                 witnessGenSig->getAsString(), origGenSig->getAsString());
      return true;
    }

    SmallVector<Requirement, 4> witnessRequirements(
        witnessGenSig.getRequirements().begin(),
        witnessGenSig.getRequirements().end());
    witnessGenSig = buildGenericSignature(
        P.Context, origGenSig,
        /*addedGenericParams=*/{},
        std::move(witnessRequirements),
        /*allowInverses=*/false);
  }

  auto origFnType = resultOrigFn->getLoweredFunctionType();
  auto *parameterIndices = IndexSubset::get(
      P.Context, origFnType->getNumParameters(), rawParameterIndices);
  auto *resultIndices = IndexSubset::get(
      P.Context,
      origFnType->getNumResults() +
          origFnType->getNumIndirectMutatingParameters(),
      rawResultIndices);

  resultConfig = AutoDiffConfig(parameterIndices, resultIndices, witnessGenSig);
  return false;
}

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Existential init/deinit instructions
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitDeinitExistentialAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeinitExistentialAddr(InstLoc, Val);
  return false; // Success
}

bool SILInstructionParserVisitor::visitDeinitExistentialValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeinitExistentialValue(InstLoc, Val);
  return false; // Success
}

bool SILInstructionParserVisitor::visitInitExistentialAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  CanType Ty;
  SourceLoc TyLoc;

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
      P.parseASTType(Ty, TyLoc) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto archetype =
      ExistentialArchetypeType::get(Val->getType().getASTType())
          ->getCanonicalType();

  auto &F = B.getFunction();
  SILType LoweredTy =
      F.getLoweredType(Lowering::AbstractionPattern(archetype), Ty)
          .getAddressType();

  ArrayRef<ProtocolConformanceRef> conformances =
      collectExistentialConformances(P.P, Ty, TyLoc,
                                     Val->getType().getASTType());

  ResultVal = B.createInitExistentialAddr(InstLoc, Val, Ty, LoweredTy,
                                          conformances);
  return false; // Success
}

bool SILInstructionParserVisitor::visitInitExistentialValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  CanType FormalConcreteTy;
  SILType ExistentialTy;
  SourceLoc TyLoc;

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
      P.parseASTType(FormalConcreteTy, TyLoc) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILType(ExistentialTy) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ArrayRef<ProtocolConformanceRef> conformances =
      collectExistentialConformances(P.P, FormalConcreteTy, TyLoc,
                                     ExistentialTy.getASTType());

  ResultVal = B.createInitExistentialValue(
      InstLoc, ExistentialTy, FormalConcreteTy, Val, conformances);
  return false; // Success
}

bool SILInstructionParserVisitor::visitInitExistentialRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  CanType FormalConcreteTy;
  SILType ExistentialTy;
  SourceLoc TyLoc;

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":") ||
      P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
      P.parseASTType(FormalConcreteTy, TyLoc) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILType(ExistentialTy))
    return true;

  ValueOwnershipKind forwardingOwnership = Val->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ArrayRef<ProtocolConformanceRef> conformances =
      collectExistentialConformances(P.P, FormalConcreteTy, TyLoc,
                                     ExistentialTy.getASTType());

  ResultVal =
      B.createInitExistentialRef(InstLoc, ExistentialTy, FormalConcreteTy, Val,
                                 conformances, forwardingOwnership);
  return false; // Success
}

bool SILInstructionParserVisitor::visitInitExistentialMetatypeInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILType ExistentialTy;
  SourceLoc TyLoc;

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILType(ExistentialTy, TyLoc) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto baseExType = ExistentialTy.getASTType();
  auto formalConcreteType = Val->getType().getASTType();
  while (auto instExType = dyn_cast<ExistentialMetatypeType>(baseExType)) {
    baseExType = instExType.getInstanceType();
    formalConcreteType =
        cast<MetatypeType>(formalConcreteType).getInstanceType();
  }

  ArrayRef<ProtocolConformanceRef> conformances =
      collectExistentialConformances(P.P, formalConcreteType, TyLoc, baseExType);

  ResultVal = B.createInitExistentialMetatype(InstLoc, Val, ExistentialTy,
                                              conformances);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Dynamic method & block storage helpers
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitDynamicMethodBranchInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILDeclRef Member;
  Identifier TrueName, FalseName;
  SourceLoc TrueLoc, FalseLoc;

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILDeclRef(Member) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILIdentifier(TrueName, TrueLoc, diag::expected_sil_block_name) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILIdentifier(FalseName, FalseLoc,
                           diag::expected_sil_block_name) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDynamicMethodBranch(
      InstLoc, Val, Member, P.getBBForReference(TrueName, TrueLoc),
      P.getBBForReference(FalseName, FalseLoc));
  return false; // Success
}

bool SILInstructionParserVisitor::visitProjectBlockStorageInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createProjectBlockStorage(InstLoc, Val);
  return false; // Success
}

bool SILInstructionParserVisitor::visitInitBlockStorageHeaderInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Storage;
  Identifier InvokeTok, TypeTok;
  SourceLoc InvokeLoc, TypeLoc;

  SILParser::UnresolvedValueName invokeName;
  SILType invokeTy;
  GenericSignature invokeGenericSig;
  GenericParamList *invokeGenericParams = nullptr;

  SILType blockType;
  SmallVector<ParsedSubstitution, 4> parsedSubs;

  if (P.parseTypedValueRef(Storage, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILIdentifier(InvokeTok, InvokeLoc, diag::expected_tok_in_sil_instr,
                           "invoke") ||
      P.parseValueName(invokeName) || P.parseSubstitutions(parsedSubs) ||
      P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":") ||
      P.parseSILType(invokeTy, invokeGenericSig, invokeGenericParams) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILIdentifier(TypeTok, TypeLoc, diag::expected_tok_in_sil_instr,
                           "type") ||
      P.parseSILType(blockType) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (InvokeTok.str() != "invoke") {
    P.P.diagnose(InvokeLoc, diag::expected_tok_in_sil_instr, "invoke");
    return true;
  }
  if (TypeTok.str() != "type") {
    P.P.diagnose(TypeLoc, diag::expected_tok_in_sil_instr, "type");
    return true;
  }

  auto invokeVal = P.getLocalValue(invokeName, invokeTy, InstLoc, B);

  SubstitutionMap subMap;
  if (!parsedSubs.empty()) {
    if (!invokeGenericSig) {
      P.P.diagnose(TypeLoc, diag::sil_substitutions_on_non_polymorphic_type);
      return true;
    }
    subMap = getApplySubstitutionsFromParsed(P, invokeGenericSig, parsedSubs);
    if (!subMap)
      return true;
  }

  ResultVal = B.createInitBlockStorageHeader(InstLoc, Storage, invokeVal,
                                             blockType, subMap);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Differentiable / Linear function utilities
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitDifferentiableFunctionInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SmallVector<unsigned, 8> rawParameterIndices;
  SmallVector<unsigned, 8> rawResultIndices;

  if (parseIndexListVisitor(P.P, "parameters", rawParameterIndices,
                            diag::sil_autodiff_expected_parameter_index) ||
      parseIndexListVisitor(P.P, "results", rawResultIndices,
                            diag::sil_autodiff_expected_result_index))
    return true;

  SILValue original;
  SourceLoc originalLoc;
  if (P.parseTypedValueRef(original, originalLoc, B))
    return true;

  auto fnType = original->getType().getAs<SILFunctionType>();
  if (!fnType) {
    P.P.diagnose(originalLoc,
                 diag::sil_inst_autodiff_expected_function_type_operand);
    return true;
  }

  std::optional<std::pair<SILValue, SILValue>> derivativeFns = std::nullopt;
  if (P.P.Tok.is(tok::identifier) &&
      P.P.Tok.getText() == "with_derivative") {
    P.P.consumeToken(tok::identifier);
    derivativeFns = std::make_pair(SILValue(), SILValue());
    if (P.P.parseToken(tok::l_brace,
                       diag::sil_inst_autodiff_operand_list_expected_lbrace) ||
        P.parseTypedValueRef(derivativeFns->first, B) ||
        P.P.parseToken(tok::comma,
                       diag::sil_inst_autodiff_operand_list_expected_comma) ||
        P.parseTypedValueRef(derivativeFns->second, B) ||
        P.P.parseToken(tok::r_brace,
                       diag::sil_inst_autodiff_operand_list_expected_rbrace))
      return true;
  }

  ValueOwnershipKind forwardingOwnership(OwnershipKind::None);
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto *parameterIndices =
      IndexSubset::get(P.P.Context, fnType->getNumParameters(), rawParameterIndices);
  auto *resultIndices = IndexSubset::get(
      P.P.Context,
      fnType->getNumResults() + fnType->getNumIndirectMutatingParameters(),
      rawResultIndices);

  if (forwardingOwnership != OwnershipKind::None) {
    ResultVal = B.createDifferentiableFunction(
        InstLoc, parameterIndices, resultIndices, original, derivativeFns,
        forwardingOwnership);
  } else {
    ResultVal = B.createDifferentiableFunction(
        InstLoc, parameterIndices, resultIndices, original, derivativeFns);
  }
  return false; // Success
}

bool SILInstructionParserVisitor::visitLinearFunctionInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SmallVector<unsigned, 8> rawParameterIndices;

  if (parseIndexListVisitor(P.P, "parameters", rawParameterIndices,
                            diag::sil_autodiff_expected_parameter_index))
    return true;

  SILValue original;
  SourceLoc originalLoc;
  if (P.parseTypedValueRef(original, originalLoc, B))
    return true;

  auto fnType = original->getType().getAs<SILFunctionType>();
  if (!fnType) {
    P.P.diagnose(originalLoc,
                 diag::sil_inst_autodiff_expected_function_type_operand);
    return true;
  }

  std::optional<SILValue> transpose = std::nullopt;
  if (P.P.Tok.is(tok::identifier) &&
      P.P.Tok.getText() == "with_transpose") {
    P.P.consumeToken(tok::identifier);
    transpose = SILValue();
    if (P.parseTypedValueRef(*transpose, B))
      return true;
  }

  ValueOwnershipKind forwardingOwnership = original->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto *parameterIndices =
      IndexSubset::get(P.P.Context, fnType->getNumParameters(), rawParameterIndices);

  if (forwardingOwnership != OwnershipKind::None) {
    ResultVal = B.createLinearFunction(
        InstLoc, parameterIndices, original, forwardingOwnership, transpose);
  } else {
    ResultVal =
        B.createLinearFunction(InstLoc, parameterIndices, original, transpose);
  }
  return false; // Success
}

bool SILInstructionParserVisitor::visitDifferentiableFunctionExtractInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  NormalDifferentiableFunctionTypeComponent extractee;
  StringRef extracteeNames[3] = {"original", "jvp", "vjp"};

  if (P.P.parseToken(
          tok::l_square,
          diag::sil_inst_autodiff_expected_differentiable_extractee_kind) ||
      P.parseSILIdentifierSwitch(
          extractee, extracteeNames,
          diag::sil_inst_autodiff_expected_differentiable_extractee_kind) ||
      P.P.parseToken(tok::r_square,
                     diag::sil_autodiff_expected_rsquare, "extractee kind"))
    return true;

  SILValue functionOperand;
  if (P.parseTypedValueRef(functionOperand, B))
    return true;

  std::optional<SILType> extracteeType = std::nullopt;
  if (P.P.consumeIf(tok::kw_as)) {
    extracteeType = SILType();
    if (P.parseSILType(*extracteeType))
      return true;
  }

  ValueOwnershipKind forwardingOwnership = functionOperand->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDifferentiableFunctionExtract(
      InstLoc, extractee, functionOperand, forwardingOwnership, extracteeType);
  return false; // Success
}

bool SILInstructionParserVisitor::visitLinearFunctionExtractInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  LinearDifferentiableFunctionTypeComponent extractee;
  StringRef extracteeNames[2] = {"original", "transpose"};

  if (P.P.parseToken(
          tok::l_square,
          diag::sil_inst_autodiff_expected_linear_extractee_kind) ||
      P.parseSILIdentifierSwitch(
          extractee, extracteeNames,
          diag::sil_inst_autodiff_expected_linear_extractee_kind) ||
      P.P.parseToken(tok::r_square,
                     diag::sil_autodiff_expected_rsquare, "extractee kind"))
    return true;

  SILValue functionOperand;
  if (P.parseTypedValueRef(functionOperand, B))
    return true;

  ValueOwnershipKind forwardingOwnership = functionOperand->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(forwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createLinearFunctionExtract(
      InstLoc, extractee, functionOperand, forwardingOwnership);
  return false; // Success
}

bool SILInstructionParserVisitor::visitDifferentiabilityWitnessFunctionInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  DifferentiabilityWitnessFunctionKind witnessKind;
  StringRef witnessKindNames[3] = {"jvp", "vjp", "transpose"};
  if (P.P.parseToken(
          tok::l_square,
          diag::
              sil_inst_autodiff_expected_differentiability_witness_kind) ||
      P.parseSILIdentifierSwitch(
          witnessKind, witnessKindNames,
          diag::
              sil_inst_autodiff_expected_differentiability_witness_kind) ||
      P.P.parseToken(tok::r_square, diag::sil_autodiff_expected_rsquare,
                     "differentiability witness function kind"))
    return true;

  SourceLoc keyStartLoc = P.P.Tok.getLoc();
  DifferentiabilityKind diffKind;
  AutoDiffConfig config;
  SILFunction *originalFn = nullptr;

  if (parseSILDifferentiabilityWitnessConfigAndFunctionVisitor(
          P.P, P, InstLoc, diffKind, config, originalFn))
    return true;

  auto *witness = P.SILMod.lookUpDifferentiabilityWitness(
      {originalFn->getName(), diffKind, config});
  if (!witness) {
    P.P.diagnose(keyStartLoc, diag::sil_diff_witness_undefined);
    return true;
  }

  std::optional<SILType> functionType = std::nullopt;
  if (P.P.consumeIf(tok::kw_as)) {
    functionType = SILType();
    if (P.parseSILType(*functionType))
      return true;
  }

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDifferentiabilityWitnessFunction(
      InstLoc, witnessKind, witness, functionType);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Async continuation helpers
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitAwaitAsyncContinuationInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  Identifier ResumeName, ErrorName;
  SourceLoc ResumeLoc, ErrorLoc;

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.P.parseSpecificIdentifier("resume", diag::expected_tok_in_sil_instr,
                                  "resume") ||
      P.parseSILIdentifier(ResumeName, ResumeLoc,
                           diag::expected_sil_block_name))
    return true;

  bool hasErrorBB = false;
  if (P.P.consumeIf(tok::comma)) {
    if (P.P.parseSpecificIdentifier("error", diag::expected_tok_in_sil_instr,
                                    "error") ||
        P.parseSILIdentifier(ErrorName, ErrorLoc,
                             diag::expected_sil_block_name) ||
        P.parseSILDebugLocation(InstLoc, B))
      return true;
    hasErrorBB = true;
  } else if (P.parseSILDebugLocation(InstLoc, B)) {
    return true;
  }

  auto *resumeBB = P.getBBForReference(ResumeName, ResumeLoc);
  SILBasicBlock *errorBB = nullptr;
  if (hasErrorBB)
    errorBB = P.getBBForReference(ErrorName, ErrorLoc);

  ResultVal =
      B.createAwaitAsyncContinuation(InstLoc, Val, resumeBB, errorBB);
  return false; // Success
}

bool SILInstructionParserVisitor::visitGetAsyncContinuationInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool throws = false;
  if (P.P.consumeIf(tok::l_square)) {
    if (P.P.parseToken(tok::kw_throws, diag::expected_tok_in_sil_instr,
                       "throws") ||
        P.P.parseToken(tok::r_square, diag::expected_tok_in_sil_instr, "]"))
      return true;
    throws = true;
  }

  CanType resumeTy;
  if (P.parseASTType(resumeTy) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createGetAsyncContinuation(InstLoc, resumeTy, throws);
  return false; // Success
}

bool SILInstructionParserVisitor::visitGetAsyncContinuationAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool throws = false;
  if (P.P.consumeIf(tok::l_square)) {
    if (P.P.parseToken(tok::kw_throws, diag::expected_tok_in_sil_instr,
                       "throws") ||
        P.P.parseToken(tok::r_square, diag::expected_tok_in_sil_instr, "]"))
      return true;
    throws = true;
  }

  CanType resumeTy;
  if (P.parseASTType(resumeTy))
    return true;

  SILValue resumeBuffer;
  if (P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseTypedValueRef(resumeBuffer, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createGetAsyncContinuationAddr(InstLoc, resumeBuffer,
                                               resumeTy, throws);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Miscellaneous
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitHasSymbolInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILDeclRef DeclRef;

  if (P.parseSILDeclRef(DeclRef) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createHasSymbol(InstLoc, DeclRef.getDecl());
  return false; // Success
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE310_INC
