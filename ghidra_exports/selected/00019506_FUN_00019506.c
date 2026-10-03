
LONG __fastcall FUN_00019506(int param_1)

{
  LONG LVar1;
  uint uVar2;
  LONG *unaff_EDI;
  
  LVar1 = InterlockedIncrement(unaff_EDI);
  uVar2 = 0;
  if (((*(byte *)(param_1 + 0x110) & 4) != 0) &&
     (uVar2 = (uint)((*(byte *)(param_1 + 0x10c) & 1) != 0), *(int *)(param_1 + 0x1ac) != 0)) {
    uVar2 = uVar2 + 1;
  }
  if (LVar1 == uVar2 + 2) {
    KeClearEvent(*(undefined4 *)(param_1 + 0x1b4));
  }
  if (LVar1 == 2) {
    KeClearEvent(*(undefined4 *)(param_1 + 0x70));
  }
  return LVar1;
}

