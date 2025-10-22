//===--- Phase3_12_Dynamic.inc - Phase 3-12 Visit Methods -*- C++ -*-===//
//
// Phase 3-12: Dynamic / Async follow-up instructions (4 instructions)
//   - Dynamic function references
//   - Pack-based dynamic indexing
//   - Executor hopping helpers
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE312_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE312_INC

//===----------------------------------------------------------------------===//
// Dynamic function references
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitDynamicFunctionRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILFunction *Fn;

  if (P.parseSILFunctionRef(InstLoc, Fn) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (!Fn->isDynamicallyReplaceable()) {
    if (!Fn->empty()) {
      P.P.diagnose(P.P.Tok, diag::expected_dynamic_func_attr);
      return true;
    }
    Fn->setIsDynamic();
  }

  ResultVal = B.createDynamicFunctionRef(InstLoc, Fn);
  return false; // Success
}

bool SILInstructionParserVisitor::visitPreviousDynamicFunctionRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILFunction *Fn;

  if (P.parseSILFunctionRef(InstLoc, Fn) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createPreviousDynamicFunctionRef(InstLoc, Fn);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Dynamic pack indexing
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitDynamicPackIndexInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue indexValue;
  CanPackType packType;

  if (P.parseValueRef(indexValue,
                      SILType::getBuiltinWordType(P.P.Context),
                      InstLoc, B) ||
      P.parseVerbatim("of") ||
      P.P.parseToken(tok::sil_dollar, diag::expected_tok_in_sil_instr, "$") ||
      P.parseASTPackType(packType) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDynamicPackIndex(InstLoc, indexValue, packType);
  return false; // Success
}

//===----------------------------------------------------------------------===//
// Executor hopping helpers
//===----------------------------------------------------------------------===//

bool SILInstructionParserVisitor::visitHopToExecutorInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  bool mandatory = false;
  SILValue executor;

  if (parseSILOptional(mandatory, P, "mandatory") ||
      P.parseTypedValueRef(executor, B) ||
      P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createHopToExecutor(InstLoc, executor, mandatory);
  return false; // Success
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE312_INC
