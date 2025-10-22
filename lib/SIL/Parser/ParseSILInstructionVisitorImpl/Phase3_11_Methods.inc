//===--- Phase3_11_Methods.inc - Phase 3-11 Visit Methods -*- C++ -*-===//
//
// Phase 3-11: Method lookup & memory helpers (12 instructions)
//   - Class/Super/ObjC/Witness method lookup
//   - BindMemory/RebindMemory utilities
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE311_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE311_INC

namespace {

static bool parseMethodLookup(SILParser &P, SILBuilder &B,
                              SILInstruction *&ResultVal, SourceLoc OpcodeLoc,
                              SILInstructionKind Opcode) {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;
  SILDeclRef Member;
  SILType MethodTy;
  SourceLoc TyLoc;

  if (P.parseTypedValueRef(Val, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ","))
    return true;

  if (P.parseSILDeclRef(Member, true))
    return true;

  if (P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILType(MethodTy, TyLoc) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  switch (Opcode) {
  default:
    llvm_unreachable("Unhandled method lookup opcode");
  case SILInstructionKind::ClassMethodInst:
    ResultVal = B.createClassMethod(InstLoc, Val, Member, MethodTy);
    break;
  case SILInstructionKind::SuperMethodInst:
    ResultVal = B.createSuperMethod(InstLoc, Val, Member, MethodTy);
    break;
  case SILInstructionKind::ObjCMethodInst:
    ResultVal = B.createObjCMethod(InstLoc, Val, Member, MethodTy);
    break;
  case SILInstructionKind::ObjCSuperMethodInst:
    ResultVal = B.createObjCSuperMethod(InstLoc, Val, Member, MethodTy);
    break;
  }

  return false; // Success
}

} // end anonymous namespace

bool SILInstructionParserVisitor::visitClassMethodInst() {
  return parseMethodLookup(P, B, ResultVal, OpcodeLoc,
                           SILInstructionKind::ClassMethodInst);
}

bool SILInstructionParserVisitor::visitSuperMethodInst() {
  return parseMethodLookup(P, B, ResultVal, OpcodeLoc,
                           SILInstructionKind::SuperMethodInst);
}

bool SILInstructionParserVisitor::visitObjCMethodInst() {
  return parseMethodLookup(P, B, ResultVal, OpcodeLoc,
                           SILInstructionKind::ObjCMethodInst);
}

bool SILInstructionParserVisitor::visitObjCSuperMethodInst() {
  return parseMethodLookup(P, B, ResultVal, OpcodeLoc,
                           SILInstructionKind::ObjCSuperMethodInst);
}

bool SILInstructionParserVisitor::visitWitnessMethodInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  CanType LookupTy;
  SILDeclRef Member;
  SILType MethodTy;
  SourceLoc TyLoc;

  if (P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
      P.parseASTType(LookupTy) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ","))
    return true;

  if (P.parseSILDeclRef(Member, true))
    return true;

  SILValue Operand;
  if (P.P.Tok.is(tok::comma)) {
    P.P.consumeToken(tok::comma);
    if (P.parseTypedValueRef(Operand, B))
      return true;
  }

  if (P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":") ||
      P.parseSILType(MethodTy, TyLoc) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto *proto =
      dyn_cast<ProtocolDecl>(Member.getDecl()->getDeclContext());
  if (!proto) {
    P.P.diagnose(TyLoc, diag::sil_witness_method_not_protocol);
    return true;
  }

  auto conformance = lookupConformance(LookupTy, proto);
  if (conformance.isInvalid()) {
    P.P.diagnose(TyLoc, diag::sil_witness_method_type_does_not_conform);
    return true;
  }

  ResultVal =
      B.createWitnessMethod(InstLoc, LookupTy, conformance, Member, MethodTy);
  return false; // Success
}

bool SILInstructionParserVisitor::visitBindMemoryInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Base;
  SILValue IndexVal;
  SILType EltTy;

  if (P.parseTypedValueRef(Base, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseTypedValueRef(IndexVal, B) ||
      P.parseVerbatim("to") ||
      P.parseSILType(EltTy) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createBindMemory(InstLoc, Base, IndexVal, EltTy);
  return false; // Success
}

bool SILInstructionParserVisitor::visitRebindMemoryInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Base;
  SILValue Token;

  if (P.parseTypedValueRef(Base, B) ||
      P.parseVerbatim("to") ||
      P.parseTypedValueRef(Token, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createRebindMemory(InstLoc, Base, Token);
  return false; // Success
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE311_INC
