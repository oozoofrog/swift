//===--- Phase3_19_Remaining.inc - Phase 3-19 Visit Methods -*- C++ -*-===//
//
// Phase 3-19: Final reference-counting & remaining instructions (47 total)
//   - Reference counting and bridging utilities
//   - Lifetime/borrow cleanup helpers
//   - Copy/move convenience instructions
//   - Weak/Unowned load & store support
//   - Aggregate utilities, literals, builtins, cast branches
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE319_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE319_INC

namespace {

static bool parseOptionalAtomicity(SILParser &Parser, StringRef OpcodeName,
                                   Atomicity &Atomic) {
  StringRef AttrName;
  SourceLoc AttrLoc;
  if (!parseSILOptional(AttrName, AttrLoc, Parser))
    return false; // No attribute present.

  if (AttrName.empty())
    return true; // Parsing failure already diagnosed.

  if (AttrName == "nonatomic") {
    Atomic = Atomicity::NonAtomic;
    return false;
  }

  Parser.P.diagnose(AttrLoc, diag::sil_invalid_attribute_for_instruction,
                    AttrName, OpcodeName);
  return true;
}

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Simple unary instructions
//===----------------------------------------------------------------------===//

#define DEFINE_SIMPLE_UNARY_VISITOR(ID)                                        \
  bool SILInstructionParserVisitor::visit##ID##Inst() {                        \
    SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);      \
    SILValue Operand;                                                          \
    if (P.parseTypedValueRef(Operand, B) ||                                    \
        P.parseSILDebugLocation(InstLoc, B))                                   \
      return true;                                                             \
    ResultVal = B.create##ID(InstLoc, Operand);                                \
    return false; /* Success */                                                \
  }

DEFINE_SIMPLE_UNARY_VISITOR(ClassifyBridgeObject)
DEFINE_SIMPLE_UNARY_VISITOR(ValueToBridgeObject)
DEFINE_SIMPLE_UNARY_VISITOR(FixLifetime)
DEFINE_SIMPLE_UNARY_VISITOR(EndLifetime)
DEFINE_SIMPLE_UNARY_VISITOR(ExtendLifetime)
DEFINE_SIMPLE_UNARY_VISITOR(CopyBlock)
DEFINE_SIMPLE_UNARY_VISITOR(IsUnique)
DEFINE_SIMPLE_UNARY_VISITOR(DestroyAddr)
DEFINE_SIMPLE_UNARY_VISITOR(CopyValue)
DEFINE_SIMPLE_UNARY_VISITOR(ExplicitCopyValue)
DEFINE_SIMPLE_UNARY_VISITOR(EndBorrow)
DEFINE_SIMPLE_UNARY_VISITOR(DestructureStruct)
DEFINE_SIMPLE_UNARY_VISITOR(DestructureTuple)
DEFINE_SIMPLE_UNARY_VISITOR(ExtractExecutor)
DEFINE_SIMPLE_UNARY_VISITOR(FunctionExtractIsolation)
DEFINE_SIMPLE_UNARY_VISITOR(EndInitLetRef)
DEFINE_SIMPLE_UNARY_VISITOR(UnownedCopyValue)
DEFINE_SIMPLE_UNARY_VISITOR(WeakCopyValue)
DEFINE_SIMPLE_UNARY_VISITOR(StrongCopyUnmanagedValue)
DEFINE_SIMPLE_UNARY_VISITOR(StrongCopyUnownedValue)
DEFINE_SIMPLE_UNARY_VISITOR(StrongCopyWeakValue)
DEFINE_SIMPLE_UNARY_VISITOR(IgnoredUse)

#undef DEFINE_SIMPLE_UNARY_VISITOR

//===----------------------------------------------------------------------===//
// Reference counting instructions with optional atomicity
//===----------------------------------------------------------------------===//

#define DEFINE_REFCOUNT_VISITOR(ID)                                            \
  bool SILInstructionParserVisitor::visit##ID##Inst() {                        \
    SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);      \
    Atomicity Atomic = Atomicity::Atomic;                                      \
    if (parseOptionalAtomicity(P, OpcodeName, Atomic))                         \
      return true;                                                             \
    SILValue Operand;                                                          \
    if (P.parseTypedValueRef(Operand, B) ||                                    \
        P.parseSILDebugLocation(InstLoc, B))                                   \
      return true;                                                             \
    ResultVal = B.create##ID(InstLoc, Operand, Atomic);                        \
    return false; /* Success */                                                \
  }

DEFINE_REFCOUNT_VISITOR(AutoreleaseValue)
DEFINE_REFCOUNT_VISITOR(UnmanagedAutoreleaseValue)
DEFINE_REFCOUNT_VISITOR(UnmanagedReleaseValue)
DEFINE_REFCOUNT_VISITOR(UnmanagedRetainValue)
DEFINE_REFCOUNT_VISITOR(StrongRetain)
DEFINE_REFCOUNT_VISITOR(StrongRelease)
DEFINE_REFCOUNT_VISITOR(StrongRetainUnowned)
DEFINE_REFCOUNT_VISITOR(RetainValue)
DEFINE_REFCOUNT_VISITOR(ReleaseValue)
DEFINE_REFCOUNT_VISITOR(RetainValueAddr)
DEFINE_REFCOUNT_VISITOR(ReleaseValueAddr)
DEFINE_REFCOUNT_VISITOR(UnownedRetain)
DEFINE_REFCOUNT_VISITOR(UnownedRelease)

#undef DEFINE_REFCOUNT_VISITOR

//===----------------------------------------------------------------------===//
// Load from weak/unowned storage
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitLoadWeakInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool IsTake = false;
  SILValue Address;
  SourceLoc AddressLoc;

  if (parseSILOptional(IsTake, P, "take") ||
      P.parseTypedValueRef(Address, AddressLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!Address->getType().is<WeakStorageType>()) {
    P.P.diagnose(AddressLoc, diag::sil_operand_not_ref_storage_address,
                 "source", OpcodeName, ReferenceOwnership::Weak);
    return true;
  }

  ResultVal = B.createLoadWeak(InstLoc, Address, IsTake_t(IsTake));
  return false; // Success
}

bool SILInstructionParserVisitor::visitLoadUnownedInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool IsTake = false;
  SILValue Address;
  SourceLoc AddressLoc;

  if (parseSILOptional(IsTake, P, "take") ||
      P.parseTypedValueRef(Address, AddressLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!Address->getType().is<UnownedStorageType>()) {
    P.P.diagnose(AddressLoc, diag::sil_operand_not_ref_storage_address,
                 "source", OpcodeName, ReferenceOwnership::Unowned);
    return true;
  }

  ResultVal = B.createLoadUnowned(InstLoc, Address, IsTake_t(IsTake));
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Store to weak/unowned storage
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitStoreWeakInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILParser::UnresolvedValueName From;
  Identifier ToToken;
  SourceLoc ToLoc;
  SILValue DestAddr;
  SourceLoc DestLoc;
  bool IsInit = false;

  if (P.parseValueName(From) ||
      P.parseSILIdentifier(ToToken, ToLoc, diag::expected_tok_in_sil_instr,
                           "to") ||
      parseSILOptional(IsInit, P, "init") ||
      P.parseTypedValueRef(DestAddr, DestLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (ToToken.str() != "to") {
    P.P.diagnose(ToLoc, diag::expected_tok_in_sil_instr, "to");
    return true;
  }

  if (!DestAddr->getType().isAddress()) {
    P.P.diagnose(DestLoc, diag::sil_operand_not_address, "destination",
                 OpcodeName);
    return true;
  }

  auto RefType = DestAddr->getType().getAs<WeakStorageType>();
  if (!RefType) {
    P.P.diagnose(DestLoc, diag::sil_operand_not_ref_storage_address,
                 "destination", OpcodeName, ReferenceOwnership::Weak);
    return true;
  }

  auto ValueTy = SILType::getPrimitiveObjectType(RefType.getReferentType());
  SILValue FromVal = P.getLocalValue(From, ValueTy, InstLoc, B);
  ResultVal = B.createStoreWeak(InstLoc, FromVal, DestAddr,
                                IsInitialization_t(IsInit));
  return false; // Success
}

bool SILInstructionParserVisitor::visitStoreUnownedInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILParser::UnresolvedValueName From;
  Identifier ToToken;
  SourceLoc ToLoc;
  SILValue DestAddr;
  SourceLoc DestLoc;
  bool IsInit = false;

  if (P.parseValueName(From) ||
      P.parseSILIdentifier(ToToken, ToLoc, diag::expected_tok_in_sil_instr,
                           "to") ||
      parseSILOptional(IsInit, P, "init") ||
      P.parseTypedValueRef(DestAddr, DestLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (ToToken.str() != "to") {
    P.P.diagnose(ToLoc, diag::expected_tok_in_sil_instr, "to");
    return true;
  }

  if (!DestAddr->getType().isAddress()) {
    P.P.diagnose(DestLoc, diag::sil_operand_not_address, "destination",
                 OpcodeName);
    return true;
  }

  auto RefType = DestAddr->getType().getAs<UnownedStorageType>();
  if (!RefType) {
    P.P.diagnose(DestLoc, diag::sil_operand_not_ref_storage_address,
                 "destination", OpcodeName, ReferenceOwnership::Unowned);
    return true;
  }

  auto ValueTy = SILType::getPrimitiveObjectType(RefType.getReferentType());
  SILValue FromVal = P.getLocalValue(From, ValueTy, InstLoc, B);
  ResultVal = B.createStoreUnowned(InstLoc, FromVal, DestAddr,
                                   IsInitialization_t(IsInit));
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Dealloc / aggregate utilities
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitDeallocBoxInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool IsDeadEnd = false;
  SILValue Operand;

  if (parseSILOptional(IsDeadEnd, P, "dead_end") ||
      P.parseTypedValueRef(Operand, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocBox(InstLoc, Operand, IsDeadEnd_t(IsDeadEnd));
  return false; // Success
}

bool SILInstructionParserVisitor::visitProjectBoxInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue BoxValue;

  if (P.parseTypedValueRef(BoxValue, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ","))
    return true;

  if (!P.P.Tok.is(tok::integer_literal)) {
    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "integer");
    return true;
  }

  unsigned Index;
  bool Error = P.parseIntegerLiteral(P.P.Tok.getText(), 0, Index);
  (void)Error;
  assert(!Error && "project_box index should parse as integer");
  P.P.consumeToken(tok::integer_literal);

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createProjectBox(InstLoc, BoxValue, Index);
  return false; // Success
}

bool SILInstructionParserVisitor::visitTupleAddrConstructorInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  StringRef AttrName;
  SourceLoc AttrLoc;

  if (!parseSILOptional(AttrName, AttrLoc, P)) {
    P.P.diagnose(OpcodeLoc, diag::expected_tok_in_sil_instr,
                 "[init] | [assign]");
    return true;
  }

  if (AttrName != "init" && AttrName != "assign") {
    P.P.diagnose(AttrLoc, diag::expected_tok_in_sil_instr,
                 "[init] | [assign]");
    return true;
  }

  bool IsInit = AttrName == "init";
  SILValue DestValue;
  SourceLoc DestLoc;

  if (P.parseTypedValueRef(DestValue, DestLoc, B))
    return true;

  Identifier WithToken;
  SourceLoc WithLoc;
  if (P.parseSILIdentifier(WithToken, WithLoc,
                           diag::expected_tok_in_sil_instr, "with"))
    return true;
  if (WithToken.str() != "with") {
    P.P.diagnose(WithLoc, diag::expected_tok_in_sil_instr, "with");
    return true;
  }

  if (P.P.parseToken(tok::l_paren, diag::expected_tok_in_sil_instr, "("))
    return true;

  SmallVector<SILValue, 4> Elements;
  if (P.P.Tok.isNot(tok::r_paren)) {
    do {
      SILValue Elem;
      if (P.parseTypedValueRef(Elem, B))
        return true;
      Elements.push_back(Elem);
    } while (P.P.consumeIf(tok::comma));
  }

  if (P.P.parseToken(tok::r_paren, diag::expected_tok_in_sil_instr, ")"))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createTupleAddrConstructor(InstLoc, DestValue, Elements,
                                           IsInitialization_t(IsInit));
  return false; // Success
}

bool SILInstructionParserVisitor::visitMoveOnlyWrapperToCopyableBoxInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Operand;
  SourceLoc OperandLoc;

  if (P.parseTypedValueRef(Operand, OperandLoc, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!Operand->getType().is<SILBoxType>()) {
    P.P.diagnose(OperandLoc, diag::sil_box_expected, OpcodeName);
    return true;
  }

  SILFunction *Fn = Operand->getFunction();
  if (!Operand->getType().isBoxedMoveOnlyWrappedType(Fn)) {
    P.P.diagnose(OperandLoc, diag::sil_operand_has_incorrect_moveonlywrapped,
                 "operand", OpcodeName, 0);
    return true;
  }

  ResultVal = B.createMoveOnlyWrapperToCopyableBox(InstLoc, Operand);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Open existential addr
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitOpenExistentialAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  Identifier AccessKindToken;
  SourceLoc AccessKindLoc;

  if (P.parseSILIdentifier(AccessKindToken, AccessKindLoc,
                           diag::expected_tok_in_sil_instr,
                           "opened existential access kind"))
    return true;

  auto AccessKind =
      llvm::StringSwitch<std::optional<OpenedExistentialAccess>>(
          AccessKindToken.str())
          .Case("mutable_access", OpenedExistentialAccess::Mutable)
          .Case("immutable_access", OpenedExistentialAccess::Immutable)
          .Default(std::nullopt);

  if (!AccessKind) {
    P.P.diagnose(AccessKindLoc, diag::expected_tok_in_sil_instr,
                 "opened existential access kind");
    return true;
  }

  SILValue Operand;
  SILType ResultTy;
  if (P.parseTypedValueRef(Operand, B) || P.parseVerbatim("to") ||
      P.parseSILType(ResultTy) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createOpenExistentialAddr(InstLoc, Operand, ResultTy, *AccessKind);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Literals & builtins
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitIntegerLiteralInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;
  if (P.parseSILType(Ty) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ","))
    return true;

  bool Negative = false;
  if (P.P.Tok.isAnyOperator() && P.P.Tok.getText() == "-") {
    Negative = true;
    P.P.consumeToken();
  }

  if (P.P.Tok.getKind() != tok::integer_literal) {
    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "integer");
    return true;
  }

  auto IntTy = Ty.getAs<AnyBuiltinIntegerType>();
  if (!IntTy) {
    P.P.diagnose(P.P.Tok, diag::sil_integer_literal_not_integer_type);
    return true;
  }

  StringRef Text = P.prepareIntegerLiteralForParsing(P.P.Tok.getText());
  bool Error = false;
  APInt Value = IntTy->getWidth().parse(Text, 0, Negative, &Error);
  if (Error) {
    P.P.diagnose(P.P.Tok, diag::sil_integer_literal_not_well_formed, IntTy);
    return true;
  }

  P.P.consumeToken(tok::integer_literal);
  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createIntegerLiteral(InstLoc, Ty, Value);
  return false; // Success
}

SubstitutionMap getApplySubstitutionsFromParsed(
    SILParser &SP, GenericSignature genericSig,
    ArrayRef<ParsedSubstitution> parsedSubs);

bool SILInstructionParserVisitor::visitBuiltinInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  if (P.P.Tok.getKind() != tok::string_literal) {
    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "builtin name");
    return true;
  }

  StringRef RawName = P.P.Tok.getText();
  auto &Ctx = P.SILMod.getASTContext();
  Identifier BuiltinName =
      Ctx.getIdentifier(RawName.substr(1, RawName.size() - 2));
  P.P.consumeToken(tok::string_literal);

  SmallVector<ValueDecl *, 2> FoundBuiltins;
  Ctx.TheBuiltinModule->lookupMember(
      FoundBuiltins, Ctx.TheBuiltinModule, BuiltinName, Identifier());
  if (FoundBuiltins.size() != 1) {
    P.P.diagnose(InstLoc.getSourceLoc(), diag::expected_tok_in_sil_instr,
                 "builtin name");
    return true;
  }

  auto *BuiltinFunc = cast<FuncDecl>(FoundBuiltins.front());
  GenericSignature GenericSig = BuiltinFunc->getGenericSignature();

  SmallVector<ParsedSubstitution, 4> ParsedSubs;
  SubstitutionMap SubMap;
  if (P.parseSubstitutions(ParsedSubs))
    return true;

  if (!ParsedSubs.empty()) {
    if (!GenericSig) {
      P.P.diagnose(InstLoc.getSourceLoc(),
                   diag::sil_substitutions_on_non_polymorphic_type);
      return true;
    }
    SubMap = getApplySubstitutionsFromParsed(P, GenericSig, ParsedSubs);
    if (!SubMap)
      return true;
  }

  if (P.P.Tok.getKind() != tok::l_paren) {
    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "(");
    return true;
  }
  P.P.consumeToken(tok::l_paren);

  SmallVector<SILValue, 4> Arguments;
  while (true) {
    if (P.P.consumeIf(tok::r_paren))
      break;

    SILValue Arg;
    if (P.parseTypedValueRef(Arg, B))
      return true;
    Arguments.push_back(Arg);

    if (P.P.consumeIf(tok::comma))
      continue;
    if (P.P.consumeIf(tok::r_paren))
      break;

    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, ")' or ','");
    return true;
  }

  if (P.P.Tok.getKind() != tok::colon) {
    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, ":");
    return true;
  }
  P.P.consumeToken(tok::colon);

  SILType ResultTy;
  if (P.parseSILType(ResultTy))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createBuiltin(InstLoc, BuiltinName, ResultTy, SubMap, Arguments);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Checked cast addr branch
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitCheckedCastAddrBranchInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  CheckedCastInstOptions Options = P.parseCheckedCastInstOptions(nullptr);

  Identifier ConsumptionToken;
  SourceLoc ConsumptionLoc;
  if (P.parseSILIdentifier(ConsumptionToken, ConsumptionLoc,
                           diag::expected_tok_in_sil_instr,
                           "cast consumption kind"))
    return true;

  auto Consumption = llvm::StringSwitch<std::optional<CastConsumptionKind>>(
                         ConsumptionToken.str())
                         .Case("take_always", CastConsumptionKind::TakeAlways)
                         .Case("take_on_success",
                               CastConsumptionKind::TakeOnSuccess)
                         .Case("copy_on_success",
                               CastConsumptionKind::CopyOnSuccess)
                         .Default(std::nullopt);

  if (!Consumption) {
    P.P.diagnose(ConsumptionLoc, diag::expected_tok_in_sil_instr,
                 "cast consumption kind");
    return true;
  }

  CanType SourceFormalType, TargetFormalType;
  SILValue SourceAddr, DestAddr;
  auto ParseFormalTypeAndValue = [&](CanType &FormalTy,
                                     SILValue &Value) -> bool {
    return P.parseASTType(FormalTy) || P.parseVerbatim("in") ||
           P.parseTypedValueRef(Value, B);
  };

  if (ParseFormalTypeAndValue(SourceFormalType, SourceAddr) ||
      P.parseVerbatim("to") ||
      ParseFormalTypeAndValue(TargetFormalType, DestAddr))
    return true;

  Identifier SuccessName, FailureName;
  SourceLoc SuccessLoc, FailureLoc;
  if (P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILIdentifier(SuccessName, SuccessLoc,
                           diag::expected_sil_block_name) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILIdentifier(FailureName, FailureLoc,
                           diag::expected_sil_block_name))
    return true;

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto *SuccessBB = P.getBBForReference(SuccessName, SuccessLoc);
  auto *FailureBB = P.getBBForReference(FailureName, FailureLoc);

  ResultVal = B.createCheckedCastAddrBranch(
      InstLoc, Options, *Consumption, SourceAddr, SourceFormalType, DestAddr,
      TargetFormalType, SuccessBB, FailureBB);
  return false; // Success
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE319_INC
