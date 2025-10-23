//===--- Phase3_18_Switch.inc - Phase 3-18 Visit Methods -*- C++ -*-===//
//
// Phase 3-18: Switch instructions (1 instruction)
//   - SwitchValueInst
//
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE318_SWITCH_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE318_SWITCH_INC

bool SILInstructionParserVisitor::visitSwitchValueInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B))
    return true;

  SmallVector<std::pair<SILValue, SILBasicBlock *>, 4> CaseBBs;
  SILBasicBlock *DefaultBB = nullptr;

  while (!P.peekSILDebugLocation() && P.P.consumeIf(tok::comma)) {
    Identifier BBName;
    SourceLoc BBLoc;
    SILValue CaseVal;

    if (P.P.consumeIf(tok::kw_default)) {
      if (P.parseSILIdentifier(BBName, BBLoc, diag::expected_sil_block_name))
        return true;
      DefaultBB = P.getBBForReference(BBName, BBLoc);
      break;
    }

    if (P.P.consumeIf(tok::kw_case)) {
      if (P.parseValueRef(CaseVal, Val->getType(), InstLoc, B))
        return true;

      auto intTy = Val->getType().getAs<BuiltinIntegerType>();
      auto functionTy = Val->getType().getAs<SILFunctionType>();

      if (intTy) {
        if (!isa<SILUndef>(CaseVal)) {
          auto *IL = dyn_cast<IntegerLiteralInst>(CaseVal);
          if (!IL) {
            P.P.diagnose(P.P.Tok, diag::sil_integer_literal_not_integer_type);
            return true;
          }
          APInt CaseValue = IL->getValue();
          if (CaseValue.getBitWidth() != intTy->getGreatestWidth())
            CaseVal = B.createIntegerLiteral(IL->getLoc(), Val->getType(),
                                             CaseValue.zextOrTrunc(
                                                 intTy->getGreatestWidth()));
        }
      }

      if (functionTy) {
        if (!isa<SILUndef>(CaseVal)) {
          auto *FR = dyn_cast<FunctionRefInst>(CaseVal);
          if (!FR) {
            if (auto *CF = dyn_cast<ConvertFunctionInst>(CaseVal))
              FR = dyn_cast<FunctionRefInst>(CF->getOperand());
          }
          if (!FR) {
            P.P.diagnose(P.P.Tok, diag::sil_integer_literal_not_integer_type);
            return true;
          }
        }
      }

      if (P.P.parseToken(tok::colon, diag::expected_tok_in_sil_instr, ":"))
        return true;
      if (P.parseSILIdentifier(BBName, BBLoc, diag::expected_sil_block_name))
        return true;
      CaseBBs.push_back({CaseVal, P.getBBForReference(BBName, BBLoc)});
      continue;
    }

    P.P.diagnose(P.P.Tok, diag::expected_tok_in_sil_instr, "case or default");
    return true;
  }

  if (P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createSwitchValue(InstLoc, Val, DefaultBB, CaseBBs);
  return false; // Success
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE318_SWITCH_INC
