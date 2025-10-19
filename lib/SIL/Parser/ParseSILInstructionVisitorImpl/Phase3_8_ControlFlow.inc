//===--- Phase3_8_ControlFlow.inc - Phase 3-8 Visit Methods ----*- C++ -*-===//
//
// Phase 3-8: Control Flow instructions (9 instructions)
//   - Return, Throw, ThrowAddr, Unwind, Yield
//   - Branch, CondBranch, Unreachable, CondFail
//
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitReturnInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createReturn(InstLoc, Val);
  return false; // Success
}

bool SILInstructionParserVisitor::visitThrowInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createThrow(InstLoc, Val);
  return false; // Success
}

bool SILInstructionParserVisitor::visitThrowAddrInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createThrowAddr(InstLoc);
  return false; // Success
}

bool SILInstructionParserVisitor::visitUnwindInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createUnwind(InstLoc);
  return false; // Success
}

bool SILInstructionParserVisitor::visitYieldInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SmallVector<SILValue, 6> Values;

  if (P.P.consumeIf(tok::l_paren)) {
    if (!P.P.Tok.is(tok::r_paren)) {
      do {
        SILValue Val;
        if (P.parseTypedValueRef(Val, B))
          return true;
        Values.push_back(Val);
      } while (P.P.consumeIf(tok::comma));
    }

    if (P.P.parseToken(tok::r_paren, diag::expected_tok_in_sil_instr, ")"))
      return true;
  } else {
    SILValue Val;
    if (P.parseTypedValueRef(Val, B))
      return true;
    Values.push_back(Val);
  }

  Identifier ResumeName, UnwindName;
  SourceLoc ResumeLoc, UnwindLoc;
  if (P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseVerbatim("resume") ||
      P.parseSILIdentifier(ResumeName, ResumeLoc,
                           diag::expected_sil_block_name) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseVerbatim("unwind") ||
      P.parseSILIdentifier(UnwindName, UnwindLoc,
                           diag::expected_sil_block_name) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto *ResumeBB = P.getBBForReference(ResumeName, ResumeLoc);
  auto *UnwindBB = P.getBBForReference(UnwindName, UnwindLoc);
  ResultVal = B.createYield(InstLoc, Values, ResumeBB, UnwindBB);
  return false; // Success
}

bool SILInstructionParserVisitor::visitBranchInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  Identifier BBName;
  SourceLoc NameLoc;

  if (P.parseSILIdentifier(BBName, NameLoc, diag::expected_sil_block_name))
    return true;

  SmallVector<SILValue, 6> Args;
  if (P.parseSILBBArgsAtBranch(Args, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal =
      B.createBranch(InstLoc, P.getBBForReference(BBName, NameLoc), Args);
  return false; // Success
}

bool SILInstructionParserVisitor::visitCondBranchInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILParser::UnresolvedValueName Cond;
  Identifier TrueName, FalseName;
  SourceLoc TrueLoc, FalseLoc;
  SmallVector<SILValue, 6> TrueArgs, FalseArgs;

  if (P.parseValueName(Cond) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILIdentifier(TrueName, TrueLoc,
                           diag::expected_sil_block_name) ||
      P.parseSILBBArgsAtBranch(TrueArgs, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseSILIdentifier(FalseName, FalseLoc,
                           diag::expected_sil_block_name) ||
      P.parseSILBBArgsAtBranch(FalseArgs, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  auto I1Ty = SILType::getBuiltinIntegerType(1, P.SILMod.getASTContext());
  SILValue CondVal = P.getLocalValue(Cond, I1Ty, InstLoc, B);
  ResultVal = B.createCondBranch(
      InstLoc, CondVal, P.getBBForReference(TrueName, TrueLoc), TrueArgs,
      P.getBBForReference(FalseName, FalseLoc), FalseArgs);
  return false; // Success
}

bool SILInstructionParserVisitor::visitUnreachableInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createUnreachable(InstLoc);
  return false; // Success
}

bool SILInstructionParserVisitor::visitCondFailInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B))
    return true;

  SmallVector<char, 128> StringBuffer;
  StringRef Message;
  if (P.P.consumeIf(tok::comma)) {
    if (P.P.Tok.getKind() != tok::string_literal) {
      P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "string");
      return true;
    }

    SmallVector<Lexer::StringSegment, 1> Segments;
    P.P.L->getStringLiteralSegments(P.P.Tok, Segments);
    assert(Segments.size() == 1 && "expected single string literal segment");

    P.P.consumeToken(tok::string_literal);
    Message = P.P.L->getEncodedStringSegment(Segments.front(), StringBuffer);
  }

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createCondFail(InstLoc, Val, Message);
  return false; // Success
}
