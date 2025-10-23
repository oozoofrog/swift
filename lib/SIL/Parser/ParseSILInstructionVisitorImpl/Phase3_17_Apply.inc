//===--- Phase3_17_Apply.inc - Phase 3-17 Visit Methods -*- C++ -*-===//
//
// Phase 3-17: Apply-family instructions (4 instructions)
//   - ApplyInst, BeginApplyInst, PartialApplyInst, TryApplyInst
//
// Related: swift#42962 (SR-340)
//===----------------------------------------------------------------------===//

#ifndef PARSE_SIL_INSTRUCTION_VISITOR_PHASE317_APPLY_INC
#define PARSE_SIL_INSTRUCTION_VISITOR_PHASE317_APPLY_INC

bool SILInstructionParserVisitor::visitApplyInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  return P.parseCallInstruction(InstLoc, SILInstructionKind::ApplyInst, B,
                                ResultVal);
}

bool SILInstructionParserVisitor::visitBeginApplyInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  return P.parseCallInstruction(InstLoc, SILInstructionKind::BeginApplyInst, B,
                                ResultVal);
}

bool SILInstructionParserVisitor::visitPartialApplyInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  return P.parseCallInstruction(InstLoc, SILInstructionKind::PartialApplyInst,
                                B, ResultVal);
}

bool SILInstructionParserVisitor::visitTryApplyInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  return P.parseCallInstruction(InstLoc, SILInstructionKind::TryApplyInst, B,
                                ResultVal);
}

#endif // PARSE_SIL_INSTRUCTION_VISITOR_PHASE317_APPLY_INC
