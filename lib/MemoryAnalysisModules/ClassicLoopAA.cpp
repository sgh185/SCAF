#include "scaf/MemoryAnalysisModules/ClassicLoopAA.h"
#include "scaf/Utilities/CallSiteFactory.h"
#include "scaf/Utilities/GetMemOper.h"
#include "scaf/Utilities/GetSize.h"
#include "scaf/Utilities/IsVolatile.h"

using namespace llvm;
using namespace llvm::noelle;
using namespace liberty;

/// May not call down the LoopAA stack, but may top
LoopAA::ModRefResult ClassicLoopAA::getModRefInfo(CallSite CS1,
                                                  TemporalRelation Rel,
                                                  CallSite CS2, const Loop *L,
                                                  Remedies &R) {
  return ModRef;
}

/// V is never a CallSite
/// May not call down the LoopAA stack, but may top
LoopAA::ModRefResult ClassicLoopAA::getModRefInfo(CallSite CS,
                                                  TemporalRelation Rel,
                                                  const Pointer &P,
                                                  const Loop *L, Remedies &R) {
  return ModRef;
}

/// V1 is never a CallSite
/// V2 is never a CallSite
/// May not call down the LoopAA stack, but may top
LoopAA::AliasResult ClassicLoopAA::aliasCheck(const Pointer &P1,
                                              TemporalRelation Rel,
                                              const Pointer &P2, const Loop *L,
                                              Remedies &R,
                                              DesiredAliasResult dAliasRes) {
  return MayAlias;
}

LoopAA::ModRefResult ClassicLoopAA::modref(const Instruction *I1,
                                           TemporalRelation Rel,
                                           const Instruction *I2, const Loop *L,
                                           Remedies &R) {

  if (!I1->mayReadFromMemory() && !I1->mayWriteToMemory())
    return NoModRef;

  if (!I2->mayReadFromMemory() && !I2->mayWriteToMemory())
    return NoModRef;

  CallSite CS1 = getCallSite(const_cast<Instruction *>(I1));
  CallSite CS2 = getCallSite(const_cast<Instruction *>(I2));

  ModRefResult MR = ModRef;
  Remedies tmpR;

  if (!CS2.getInstruction() && !liberty::isVolatile(I2)) {
    const Value *V = liberty::getMemOper(I2);
    // corner case: be conservative
    if (!V) {
      MR = ModRef;
    } else {
      unsigned Size = liberty::getTargetSize(V, getDataLayout());
      const Pointer P(I2, V, Size);
      if (CS1.getInstruction())
        MR = ModRefResult(MR & getModRefInfo(CS1, Rel, P, L, tmpR));
      else
        MR = ModRefResult(MR & modrefSimple(I1, Rel, P, L, tmpR));
    }
  } else if (!CS1.getInstruction() && CS2.getInstruction()) {
    // corner case: be conservative
    const Value *V = liberty::getMemOper(I1);
    if (!V) {
      MR = ModRef;
    } else {
      unsigned Size = liberty::getTargetSize(V, getDataLayout());
      const Pointer P(I1, V, Size);
      ModRefResult inverse = getModRefInfo(CS2, Rev(Rel), P, L, tmpR);
      if (inverse == NoModRef)
        MR = NoModRef;
    }
  } else if (CS1.getInstruction() && CS2.getInstruction()) {
    MR = ModRefResult(MR & getModRefInfo(CS1, Rel, CS2, L, tmpR));
  }

  return LoopAA::chain(R, I1, Rel, I2, L, MR, tmpR);
}

LoopAA::ModRefResult ClassicLoopAA::modref(const Instruction *I,
                                           TemporalRelation Rel, const Value *V,
                                           unsigned Size, const Loop *L,
                                           Remedies &R) {

  CallSite CS = getCallSite(const_cast<Instruction *>(I));
  ModRefResult MR;
  Remedies tmpR;
  Remedies chainRemeds;

  if (CS.getInstruction())
    MR = getModRefInfo(CS, Rel, Pointer(V, Size), L, tmpR);
  else
    MR = modrefSimple(I, Rel, Pointer(V, Size), L, tmpR);

  return LoopAA::chain(R, I, Rel, V, Size, L, MR, tmpR);
}

LoopAA::AliasResult ClassicLoopAA::alias(const Value *V1, unsigned Size1,
                                         TemporalRelation Rel, const Value *V2,
                                         unsigned Size2, const Loop *L,
                                         Remedies &R,
                                         DesiredAliasResult dAliasRes) {

  Remedies tmpR;
  const AliasResult AR = aliasCheck(Pointer(V1, Size1), Rel, Pointer(V2, Size2),
                                    L, tmpR, dAliasRes);
  return LoopAA::chain(R, V1, Size1, Rel, V2, Size2, L, AR, tmpR, dAliasRes);
}

LoopAA::ModRefResult ClassicLoopAA::modrefSimple(const LoadInst *Load,
                                                 TemporalRelation Rel,
                                                 const Pointer &P2,
                                                 const Loop *L, Remedies &R) {
  Remedies tmpR;

  // Be conservative in the face of volatile.
  if (Load->isVolatile())
    return ModRef;

  // If the load address doesn't alias the given address, it doesn't read
  // or write the specified memory.
  const DataLayout *TD = getDataLayout();
  const Value *P1 = getMemOper(Load);
  unsigned Size1 = getSize(Load->getType(), TD);
  const AliasResult AR = aliasCheck(Pointer(Load, P1, Size1), Rel, P2, L, tmpR);
  if (AR == NoAlias) {
    DesiredAliasResult dAliasRes = DNoAlias;
    chain(R, P1, Size1, Rel, P2.ptr, P2.size, L, AR, tmpR, dAliasRes);
    return NoModRef;
  }

  // Otherwise, a load just reads.
  return Ref;
}

LoopAA::ModRefResult ClassicLoopAA::modrefSimple(const StoreInst *Store,
                                                 TemporalRelation Rel,
                                                 const Pointer &P2,
                                                 const Loop *L, Remedies &R) {

  // Be conservative in the face of volatile.
  if (Store->isVolatile())
    return ModRef;

  // If the pointer is a pointer to constant memory, then it could not have been
  // modified by this store.
  if (pointsToConstantMemory(P2.ptr, L))
    return NoModRef;

  Remedies tmpR;

  // If the store address cannot alias the pointer in question, then the
  // specified memory cannot be modified by the store.
  const DataLayout *TD = getDataLayout();
  const Value *P1 = getMemOper(Store);
  unsigned Size1 = getTargetSize(P1, TD);
  const AliasResult AR =
      aliasCheck(Pointer(Store, P1, Size1), Rel, P2, L, tmpR);
  if (AR == NoAlias) {
    DesiredAliasResult dAliasRes = DNoAlias;
    chain(R, P1, Size1, Rel, P2.ptr, P2.size, L, AR, tmpR, dAliasRes);
    return NoModRef;
  }

  // Otherwise, a store just writes.
  return Mod;
}

LoopAA::ModRefResult ClassicLoopAA::modrefSimple(const VAArgInst *VAArg,
                                                 TemporalRelation Rel,
                                                 const Pointer &P2,
                                                 const Loop *L, Remedies &R) {

  // If the pointer is a pointer to constant memory, then it could not have been
  // modified by this va_arg.
  if (pointsToConstantMemory(P2.ptr, L))
    return NoModRef;

  Remedies tmpR;

  // If the va_arg address cannot alias the pointer in question, then the
  // specified memory cannot be accessed by the va_arg.
  const Value *P1 = getMemOper(VAArg);
  const AliasResult AR =
      aliasCheck(Pointer(VAArg, P1, UnknownSize), Rel, P2, L, tmpR);
  if (AR == NoAlias) {
    DesiredAliasResult dAliasRes = DNoAlias;
    chain(R, P1, UnknownSize, Rel, P2.ptr, P2.size, L, AR, tmpR, dAliasRes);
    return NoModRef;
  }

  // Otherwise, a va_arg reads and writes.
  return ModRef;
}

LoopAA::ModRefResult ClassicLoopAA::modrefSimple(const Instruction *I,
                                                 TemporalRelation Rel,
                                                 const Pointer &P,
                                                 const Loop *L, Remedies &R) {

  switch (I->getOpcode()) {
  case Instruction::VAArg:
    return modrefSimple((const VAArgInst *)I, Rel, P, L, R);
  case Instruction::Load:
    return modrefSimple((const LoadInst *)I, Rel, P, L, R);
  case Instruction::Store:
    return modrefSimple((const StoreInst *)I, Rel, P, L, R);
  case Instruction::Call:
  case Instruction::Invoke:
    assert(false && "Calls and invokes aren't simple");
  default:
    return NoModRef;
  }
}
