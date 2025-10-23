//===--- Phase3_N_Literal.inc - Phase 3-N Literal Visit Methods -*- C++ -*-===//
//
// Phase 3-N (part 1): Literal / reference instructions (4 instructions)
//   - Float literal parsing
//   - String literal parsing
//   - Project existential box helper
//   - Function reference lookup
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE3N_LITERAL_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE3N_LITERAL_INC

bool SILInstructionParserVisitor::visitFloatLiteralInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;

  if (P.parseSILType(Ty) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ","))
    return true;

  if (P.P.Tok.getKind() != tok::integer_literal) {
    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "integer");
    return true;
  }

  auto floatTy = Ty.getAs<BuiltinFloatType>();
  if (!floatTy) {
    P.P.diagnose(P.P.Tok, diag::sil_float_literal_not_float_type);
    return true;
  }

  StringRef text = P.prepareIntegerLiteralForParsing(P.P.Tok.getText());
  APInt bits(floatTy->getBitWidth(), 0);
  bool error = text.getAsInteger(0, bits);
  assert(!error && "float_literal token did not parse as APInt?!");
  (void)error;

  if (bits.getBitWidth() != floatTy->getBitWidth())
    bits = bits.zextOrTrunc(floatTy->getBitWidth());

  APFloat value(floatTy->getAPFloatSemantics(), bits);
  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createFloatLiteral(InstLoc, Ty, value);
  P.P.consumeToken(tok::integer_literal);
  return false; // Success
}

bool SILInstructionParserVisitor::visitStringLiteralInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  if (P.P.Tok.getKind() != tok::identifier) {
    P.P.diagnose(P.P.Tok, diag::sil_string_no_encoding);
    return true;
  }

  StringLiteralInst::Encoding encoding;
  if (P.P.Tok.getText() == "utf8") {
    encoding = StringLiteralInst::Encoding::UTF8;
  } else if (P.P.Tok.getText() == "objc_selector") {
    encoding = StringLiteralInst::Encoding::ObjCSelector;
  } else if (P.P.Tok.getText() == "bytes") {
    encoding = StringLiteralInst::Encoding::Bytes;
  } else if (P.P.Tok.getText() == "oslog") {
    encoding = StringLiteralInst::Encoding::UTF8_OSLOG;
  } else {
    P.P.diagnose(P.P.Tok, diag::sil_string_invalid_encoding,
                 P.P.Tok.getText());
    return true;
  }
  P.P.consumeToken(tok::identifier);

  if (P.P.Tok.getKind() != tok::string_literal) {
    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "string");
    return true;
  }

  SmallVector<Lexer::StringSegment, 1> segments;
  P.P.L->getStringLiteralSegments(P.P.Tok, segments);
  assert(segments.size() == 1);

  P.P.consumeToken(tok::string_literal);
  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  SmallVector<char, 128> stringBuffer;
  if (encoding == StringLiteralInst::Encoding::Bytes) {
    CharSourceRange rawRange(segments.front().Loc, segments.front().Length);
    StringRef rawString = P.P.SourceMgr.extractText(rawRange);
    if (rawString.size() & 1) {
      P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr,
                   "even number of hex bytes");
      return true;
    }
    while (!rawString.empty()) {
      unsigned byte1 = llvm::hexDigitValue(rawString[0]);
      unsigned byte2 = llvm::hexDigitValue(rawString[1]);
      if (byte1 == -1U || byte2 == -1U) {
        P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr,
                     "hex bytes should contain 0-9, a-f, A-F only");
        return true;
      }
      stringBuffer.push_back(static_cast<unsigned char>((byte1 << 4) | byte2));
      rawString = rawString.drop_front(2);
    }
    ResultVal = B.createStringLiteral(InstLoc, stringBuffer, encoding);
    return false; // Success
  }

  StringRef string =
      P.P.L->getEncodedStringSegment(segments.front(), stringBuffer);
  ResultVal = B.createStringLiteral(InstLoc, string, encoding);
  return false; // Success
}

bool SILInstructionParserVisitor::visitProjectExistentialBoxInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;
  SILValue Val;

  if (P.parseSILType(Ty) || P.parseVerbatim("in") ||
      P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createProjectExistentialBox(InstLoc, Ty, Val);
  return false; // Success
}

bool SILInstructionParserVisitor::visitFunctionRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILFunction *Fn;

  if (P.parseSILFunctionRef(InstLoc, Fn) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createFunctionRef(InstLoc, Fn);
  return false; // Success
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE3N_LITERAL_INC
