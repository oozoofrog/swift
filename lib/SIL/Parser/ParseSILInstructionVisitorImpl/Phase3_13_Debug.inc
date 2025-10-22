//===--- Phase3_13_Debug.inc - Phase 3-13 Visit Methods -*- C++ -*-===//
//
// Phase 3-13: Debug / Utility instructions (18 instructions)
//   - Profiling and isolation helpers
//   - COW mutation and ownership utilities
//   - Debug/Test instrumentation
//   - Key path construction and thunk helpers
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE313_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE313_INC

SubstitutionMap getApplySubstitutionsFromParsed(
    SILParser &SP, GenericSignature genericSig,
    ArrayRef<ParsedSubstitution> parsedSubs);

//===----------------------------------------------------------------------===//
// Profiling and isolation helpers
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitIncrementProfilerCounterInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  unsigned CounterIdx;
  unsigned NumCounters;
  uint64_t Hash;

  if (P.parseInteger(CounterIdx, diag::expected_sil_profiler_counter_idx) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ","))
    return true;

  if (P.P.Tok.getKind() != tok::string_literal) {
    P.P.diagnose(P.P.Tok, diag::expected_sil_profiler_counter_pgo_func_name);
    return true;
  }
  auto FuncName = P.P.Tok.getText().drop_front().drop_back();
  P.P.consumeToken(tok::string_literal);

  if (P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseVerbatim("num_counters") ||
      P.parseInteger(NumCounters, diag::expected_sil_profiler_counter_total) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseVerbatim("hash") ||
      P.parseInteger(Hash, diag::expected_sil_profiler_counter_hash) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createIncrementProfilerCounter(InstLoc, CounterIdx, FuncName,
                                               NumCounters, Hash);
  return false; // Success
}

bool SILInstructionParserVisitor::visitMergeIsolationRegionInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SmallVector<SILValue, 4> Args;

  do {
    SILValue Val;
    if (P.parseTypedValueRef(Val, B))
      return true;
    Args.push_back(Val);
  } while (P.P.consumeIf(tok::comma));

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createMergeIsolationRegion(InstLoc, Args);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// COW mutation and ownership utilities
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitBeginCOWMutationInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool native = false;
  SILValue Operand;

  if (parseSILOptional(native, P, "native") ||
      P.parseTypedValueRef(Operand, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createBeginCOWMutation(InstLoc, Operand, native);
  return false; // Success
}

bool SILInstructionParserVisitor::visitEndCOWMutationInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool keepUnique = false;
  SILValue Operand;

  if (parseSILOptional(keepUnique, P, "keep_unique") ||
      P.parseTypedValueRef(Operand, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createEndCOWMutation(InstLoc, Operand, keepUnique);
  return false; // Success
}

bool SILInstructionParserVisitor::visitEndCOWMutationAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Operand;

  if (P.parseTypedValueRef(Operand, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createEndCOWMutationAddr(InstLoc, Operand);
  return false; // Success
}

bool SILInstructionParserVisitor::visitUncheckedOwnershipConversionInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Value;
  ValueOwnershipKind LHSKind = OwnershipKind::None;
  ValueOwnershipKind RHSKind = OwnershipKind::None;
  SourceLoc ValueLoc;

  if (P.parseTypedValueRef(Value, ValueLoc, B) ||
      P.P.parseToken(tok::comma, diag::expected_sil_colon,
                     "unchecked_ownership_conversion value ownership kind "
                     "conversion specification") ||
      P.parseSILOwnership(LHSKind) || P.parseVerbatim("to") ||
      P.parseSILOwnership(RHSKind) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (Value->getOwnershipKind() != LHSKind)
    return true;

  ResultVal = B.createUncheckedOwnershipConversion(InstLoc, Value, RHSKind);
  return false; // Success
}

bool SILInstructionParserVisitor::visitDropDeinitInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Operand;

  if (P.parseTypedValueRef(Operand, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDropDeinit(InstLoc, Operand);
  return false; // Success
}

bool SILInstructionParserVisitor::visitMarkUnresolvedReferenceBindingInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  StringRef AttrName;
  SourceLoc AttrLoc;
  SILValue Operand;

  if (!parseSILOptional(AttrName, AttrLoc, P)) {
    P.P.diagnose(InstLoc.getSourceLoc(),
                 diag::sil_markuncheckedreferencebinding_requires_attribute);
    return true;
  }

  using Kind = MarkUnresolvedReferenceBindingInst::Kind;
  Kind BindingKind = llvm::StringSwitch<Kind>(AttrName)
                         .Case("inout", Kind::InOut)
                         .Default(Kind::Invalid);

  if (BindingKind == Kind::Invalid) {
    P.P.diagnose(InstLoc.getSourceLoc(),
                 diag::sil_markuncheckedreferencebinding_invalid_attribute,
                 AttrName);
    return true;
  }

  if (P.parseTypedValueRef(Operand, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createMarkUnresolvedReferenceBindingInst(InstLoc, Operand,
                                                         BindingKind);
  return false; // Success
}

bool SILInstructionParserVisitor::visitBeginBorrowInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  auto isLexical = IsNotLexical;
  auto hasPointerEscape = DoesNotHavePointerEscape;
  auto fromVarDecl = IsNotFromVarDecl;
  auto fixed = BeginBorrowInst::IsNotFixed;
  StringRef AttrName;
  SourceLoc AttrLoc;
  SILValue Operand;

  while (parseSILOptional(AttrName, AttrLoc, P)) {
    if (AttrName == "lexical")
      isLexical = IsLexical;
    else if (AttrName == "pointer_escape")
      hasPointerEscape = HasPointerEscape;
    else if (AttrName == "var_decl")
      fromVarDecl = IsFromVarDecl;
    else if (AttrName == "fixed")
      fixed = BeginBorrowInst::IsFixed;
    else {
      P.P.diagnose(InstLoc.getSourceLoc(),
                   diag::sil_invalid_attribute_for_instruction, AttrName,
                   "begin_borrow");
      return true;
    }
  }

  SourceLoc AddrLoc;
  if (P.parseTypedValueRef(Operand, AddrLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createBeginBorrow(InstLoc, Operand, isLexical, hasPointerEscape,
                                  fromVarDecl, fixed);
  return false; // Success
}

bool SILInstructionParserVisitor::visitBorrowedFromInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue GuaranteedValue;
  SmallVector<SILValue, 4> BorrowedValues;

  if (P.parseTypedValueRef(GuaranteedValue, B) ||
      P.parseVerbatim("from") ||
      P.P.parseToken(tok::l_paren, diag::expected_tok_in_sil_instr, "("))
    return true;

  if (P.P.Tok.isNot(tok::r_paren)) {
    do {
      SILValue Val;
      if (P.parseTypedValueRef(Val, B))
        return true;
      BorrowedValues.push_back(Val);
    } while (P.P.consumeIf(tok::comma));
  }

  if (P.P.parseToken(tok::r_paren, diag::expected_tok_in_sil_instr, ")") ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createBorrowedFrom(InstLoc, GuaranteedValue, BorrowedValues);
  return false; // Success
}

bool SILInstructionParserVisitor::visitCopyBlockWithoutEscapingInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Block;
  SILValue Closure;

  if (P.parseTypedValueRef(Block, B) ||
      P.parseVerbatim("withoutEscaping") ||
      P.parseTypedValueRef(Closure, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createCopyBlockWithoutEscaping(InstLoc, Block, Closure);
  return false; // Success
}

bool SILInstructionParserVisitor::visitMarkDependenceInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  std::optional<MarkDependenceKind> DependenceKind;
  SILValue Value;
  SILValue Base;

  auto parseDependenceKind = [](StringRef Str) {
    return llvm::StringSwitch<std::optional<MarkDependenceKind>>(Str)
        .Case("unresolved", MarkDependenceKind::Unresolved)
        .Case("nonescaping", MarkDependenceKind::NonEscaping)
        .Default(std::nullopt);
  };

  if (P.parseSILQualifier<MarkDependenceKind>(DependenceKind,
                                              parseDependenceKind) ||
      P.parseTypedValueRef(Value, B) ||
      P.parseVerbatim("on") ||
      P.parseTypedValueRef(Base, B))
    return true;

  if (!DependenceKind)
    DependenceKind = MarkDependenceKind::Escaping;

  ValueOwnershipKind ForwardingOwnership = Value->getOwnershipKind();
  if (P.parseForwardingOwnershipKind(ForwardingOwnership) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createMarkDependence(InstLoc, Value, Base, ForwardingOwnership,
                             DependenceKind.value());
  return false; // Success
}

bool SILInstructionParserVisitor::visitMarkDependenceAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  std::optional<MarkDependenceKind> DependenceKind;
  SILValue Address;
  SILValue Base;

  auto parseDependenceKind = [](StringRef Str) {
    return llvm::StringSwitch<std::optional<MarkDependenceKind>>(Str)
        .Case("unresolved", MarkDependenceKind::Unresolved)
        .Case("nonescaping", MarkDependenceKind::NonEscaping)
        .Default(std::nullopt);
  };

  if (P.parseSILQualifier<MarkDependenceKind>(DependenceKind,
                                              parseDependenceKind) ||
      P.parseTypedValueRef(Address, B) ||
      P.parseVerbatim("on") ||
      P.parseTypedValueRef(Base, B))
    return true;

  if (!DependenceKind)
    DependenceKind = MarkDependenceKind::Escaping;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createMarkDependenceAddr(InstLoc, Address, Base,
                                         DependenceKind.value());
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Debug/Test instrumentation
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitDebugValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  PoisonRefs_t poisonRefs = DontPoisonRefs;
  bool hasTrace = false;
  UsesMoveableValueDebugInfo_t usesMoveableValueDebugInfo =
      DoesNotUseMoveableValueDebugInfo;
  StringRef AttrName;
  SourceLoc AttrLoc;

  while (parseSILOptional(AttrName, AttrLoc, P)) {
    if (AttrName == "poison")
      poisonRefs = PoisonRefs;
    else if (AttrName == "trace")
      hasTrace = true;
    else if (AttrName == "moveable_value_debuginfo")
      usesMoveableValueDebugInfo = UsesMoveableValueDebugInfo;
    else {
      P.P.diagnose(AttrLoc, diag::sil_invalid_attribute_for_instruction,
                   AttrName, "debug_value");
      return true;
    }
  }

  SILValue Operand;
  SILDebugVariable VarInfo;
  if (P.parseTypedValueRef(Operand, B) ||
      P.parseSILDebugVar(VarInfo) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (Operand->getType().isAddress())
    assert(!poisonRefs && "debug_value w/ address operand cannot be poison");

  if (Operand->getType().isMoveOnly())
    usesMoveableValueDebugInfo = UsesMoveableValueDebugInfo;

  ResultVal = B.createDebugValue(InstLoc, Operand, VarInfo, poisonRefs,
                                 usesMoveableValueDebugInfo, hasTrace);
  return false; // Success
}

bool SILInstructionParserVisitor::visitDebugStepInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  if (P.parseSILDebugLocation(InstLoc, B))
    return true;
  ResultVal = B.createDebugStep(InstLoc);
  return false; // Success
}

bool SILInstructionParserVisitor::visitSpecifyTestInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  if (P.P.Tok.getKind() != tok::string_literal) {
    P.P.diagnose(P.P.Tok, diag::expected_sil_specify_test_body);
    return true;
  }

  unsigned numQuotes = P.P.Tok.isMultilineString() ? 4 : 1;
  auto ArgumentsSpecification =
      P.P.Tok.getText().drop_front(numQuotes).drop_back(numQuotes).trim();
  P.P.consumeToken(tok::string_literal);

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto *tsi = B.createSpecifyTestInst(InstLoc, ArgumentsSpecification);
  SmallVector<StringRef, 4> components;
  test::getTestSpecificationComponents(ArgumentsSpecification, components);
  for (auto component : components) {
    auto offset = 0;
    size_t nameStart = StringRef::npos;
    while ((nameStart = component.find_if(
                [](char c) { return c == '%'; }, offset)) != StringRef::npos) {
      auto nameEnd = component.find_if_not(
          [](char c) { return clang::isAsciiIdentifierContinue(c); },
          nameStart + 1);
      if (nameEnd == StringRef::npos)
        nameEnd = component.size();
      auto name = component.substr(nameStart, nameEnd);
      component = component.drop_front(nameEnd);
      if (nameStart + 1 == nameEnd)
        continue;
      auto *&entry = P.LocalValues[name];
      if (entry)
        tsi->setValueForName(name, entry);
      else
        P.TestSpecsWithRefs[name].push_back(tsi);
    }
  }

  ResultVal = tsi;
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Key path construction and thunk helpers
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitKeyPathInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SmallVector<KeyPathPatternComponent, 4> components;
  SILType ResultTy;

  if (P.parseSILType(ResultTy) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ","))
    return true;

  GenericParamList *patternParams = nullptr;
  GenericSignature patternSig;
  CanType rootType;
  StringRef objcString;
  SmallVector<SILType, 4> operandTypes;

  patternParams = P.maybeParseGenericParams().getPtrOrNull();
  patternSig = handleSILGenericParams(patternParams, &P.SF);

  if (P.P.parseToken(tok::l_paren, diag::expected_tok_in_sil_instr, "("))
    return true;

  while (true) {
    Identifier componentKind;
    SourceLoc componentLoc;
    if (P.parseSILIdentifier(componentKind, componentLoc,
                             diag::sil_keypath_expected_component_kind))
      return true;

    if (componentKind.str() == "root") {
      if (P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
          P.parseASTType(rootType, patternSig, patternParams))
        return true;
    } else if (componentKind.str() == "objc") {
      auto tok = P.P.Tok;
      if (P.P.parseToken(tok::string_literal, diag::expected_tok_in_sil_instr,
                         "string literal"))
        return true;

      auto objcLiteral = tok.getText().drop_front().drop_back();
      objcString = StringRef(P.P.Context.AllocateCopy<char>(objcLiteral.begin(),
                                                           objcLiteral.end()),
                             objcLiteral.size());
    } else {
      KeyPathPatternComponent component;
      if (P.parseKeyPathPatternComponent(component, operandTypes, componentLoc,
                                         componentKind, InstLoc, patternSig,
                                         patternParams))
        return true;
      components.push_back(component);
    }

    if (!P.P.consumeIf(tok::semi))
      break;
  }

  if (P.P.parseToken(tok::r_paren, diag::expected_tok_in_sil_instr, ")") ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (rootType.isNull())
    P.P.diagnose(InstLoc.getSourceLoc(), diag::sil_keypath_no_root);

  SmallVector<ParsedSubstitution, 4> parsedSubs;
  if (P.parseSubstitutions(parsedSubs, P.ContextGenericSig,
                           P.ContextGenericParams))
    return true;

  SubstitutionMap subMap;
  if (!parsedSubs.empty()) {
    if (!patternSig) {
      P.P.diagnose(InstLoc.getSourceLoc(),
                   diag::sil_substitutions_on_non_polymorphic_type);
      return true;
    }

    subMap = getApplySubstitutionsFromParsed(P, patternSig, parsedSubs);
    if (!subMap)
      return true;
  }

  SmallVector<SILValue, 4> operands;

  if (P.P.consumeIf(tok::l_paren)) {
    while (true) {
      SILValue operand;

      if (operands.size() >= operandTypes.size() ||
          !operandTypes[operands.size()]) {
        P.P.diagnose(P.P.Tok, diag::sil_keypath_no_use_of_operand_in_pattern,
                     operands.size());
        return true;
      }

      auto ty =
          operandTypes[operands.size()].subst(P.SILMod, subMap);

      if (P.parseValueRef(operand, ty, RegularLocation(P.P.Tok.getLoc()), B))
        return true;
      operands.push_back(operand);

      if (P.P.consumeIf(tok::comma))
        continue;
      if (P.P.consumeIf(tok::r_paren))
        break;
      return true;
    }
  }

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  CanGenericSignature canSig;
  if (patternSig)
    canSig = patternSig.getCanonicalSignature();
  CanType leafType =
      components.empty() ? rootType : components.back().getComponentType();
  auto pattern = KeyPathPattern::get(B.getModule(), canSig, rootType, leafType,
                                     components, objcString);

  ResultVal = B.createKeyPath(InstLoc, pattern, subMap, operands, ResultTy);
  return false; // Success
}

bool SILInstructionParserVisitor::visitThunkInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  StringRef AttrName;
  SourceLoc AttrLoc;

  if (!parseSILOptional(AttrName, AttrLoc, P)) {
    P.P.diagnose(OpcodeLoc, diag::sil_failed_to_parse_sil_optional);
    return true;
  }

  auto kind = llvm::StringSwitch<std::optional<ThunkInst::Kind>>(AttrName)
                  .Case("identity", ThunkInst::Kind::Identity)
                  .Default(std::nullopt);
  if (!kind) {
    P.P.diagnose(OpcodeLoc, diag::sil_thunkinst_failed_to_parse_kind, AttrName);
    return true;
  }

  SILParser::UnresolvedValueName fnName;
  if (P.parseValueName(fnName))
    return true;

  SmallVector<ParsedSubstitution, 4> parsedSubs;
  if (P.parseSubstitutions(parsedSubs))
    return true;

  SILType thunkTy;
  SourceLoc typeLoc;
  GenericSignature genericSig;
  GenericParamList *genericParams = nullptr;

  if (P.P.parseToken(tok::l_paren, diag::expected_tok_in_sil_instr, "(") ||
      P.P.parseToken(tok::r_paren, diag::expected_tok_in_sil_instr, ")") ||
      P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":") ||
      P.parseSILType(thunkTy, typeLoc, genericSig, genericParams))
    return true;

  auto FTI = thunkTy.getAs<SILFunctionType>();
  if (!FTI) {
    P.P.diagnose(typeLoc, diag::expected_sil_type_kind, "be a function");
    return true;
  }

  SubstitutionMap subs;
  if (!parsedSubs.empty()) {
    if (!genericSig) {
      P.P.diagnose(typeLoc, diag::sil_substitutions_on_non_polymorphic_type);
      return true;
    }
    subs = getApplySubstitutionsFromParsed(P, genericSig, parsedSubs);
    if (!subs)
      return true;
  }

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  SILValue fnVal = P.getLocalValue(fnName, thunkTy, InstLoc, B);
  ResultVal = B.createThunk(InstLoc, fnVal, kind.value(), subs);
  return false; // Success
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE313_INC
