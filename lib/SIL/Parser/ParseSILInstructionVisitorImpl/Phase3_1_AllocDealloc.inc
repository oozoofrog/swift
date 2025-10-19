//===--- Phase3_1_AllocDealloc.inc - Phase 3-1 Visit Methods --------*- C++ -*-===//
//
// Phase 3-1: Alloc/Dealloc Batch 1 - Simple instructions (9 instructions)
//
//===----------------------------------------------------------------------===//

// AllocStackInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAllocStackInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);

  auto hasDynamicLifetime = DoesNotHaveDynamicLifetime;
  auto isLexical = IsNotLexical;
  auto isFromVarDecl = IsNotFromVarDecl;
  UsesMoveableValueDebugInfo_t usesMoveableValueDebugInfo =
      DoesNotUseMoveableValueDebugInfo;

  StringRef attributeName;
  SourceLoc attributeLoc;
  while (parseSILOptional(attributeName, attributeLoc, P)) {
    if (attributeName == "dynamic_lifetime")
      hasDynamicLifetime = HasDynamicLifetime;
    else if (attributeName == "lexical")
      isLexical = IsLexical;
    else if (attributeName == "var_decl")
      isFromVarDecl = IsFromVarDecl;
    else if (attributeName == "moveable_value_debuginfo")
      usesMoveableValueDebugInfo = UsesMoveableValueDebugInfo;
    else {
      P.P.diagnose(attributeLoc, diag::sil_invalid_attribute_for_instruction,
                   attributeName, "alloc_stack");
      return true;
    }
  }

  SILType Ty;
  if (P.parseSILType(Ty))
    return true;

  SILDebugVariable VarInfo;
  if (P.parseSILDebugVar(VarInfo) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  if (Ty.isMoveOnly())
    usesMoveableValueDebugInfo = UsesMoveableValueDebugInfo;

  // It doesn't make sense to attach a debug var info if the name is empty
  if (VarInfo.Name.size())
    ResultVal = B.createAllocStack(InstLoc, Ty, VarInfo, hasDynamicLifetime,
                                   isLexical, isFromVarDecl,
                                   usesMoveableValueDebugInfo);
  else
    ResultVal = B.createAllocStack(InstLoc, Ty, {}, hasDynamicLifetime,
                                   isLexical, isFromVarDecl,
                                   usesMoveableValueDebugInfo);
  return false; // Success
}

// AllocPackInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAllocPackInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;

  if (P.parseSILType(Ty))
    return true;

  ResultVal = B.createAllocPack(InstLoc, Ty);
  return false; // Success
}

// AllocPackMetadataInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitAllocPackMetadataInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILType Ty;

  if (P.parseSILType(Ty))
    return true;

  ResultVal = B.createAllocPackMetadata(InstLoc, Ty);
  return false; // Success
}

// DeallocStackInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocStackInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocStack(InstLoc, Val);
  return false; // Success
}

// DeallocStackRefInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocStackRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocStackRef(InstLoc, Val);
  return false; // Success
}

// DeallocPackInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocPackInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocPack(InstLoc, Val);
  return false; // Success
}

// DeallocPackMetadataInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocPackMetadataInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocPackMetadata(InstLoc, Val);
  return false; // Success
}

// DeallocRefInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Val;

  if (P.parseTypedValueRef(Val, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocRef(InstLoc, Val);
  return false; // Success
}

// DeallocPartialRefInst - Migrated from ParseSIL.cpp
bool SILInstructionParserVisitor::visitDeallocPartialRefInst() {
  SILLocation InstLoc = RegularLocation(OpcodeLoc, /*implicit*/ false);
  SILValue Metatype, Instance;

  if (P.parseTypedValueRef(Instance, B) ||
      P.P.parseToken(tok::comma, diag::expected_tok_in_sil_instr, ",") ||
      P.parseTypedValueRef(Metatype, B) || P.parseSILDebugLocation(InstLoc, B))
    return true;

  ResultVal = B.createDeallocPartialRef(InstLoc, Instance, Metatype);
  return false; // Success
}
