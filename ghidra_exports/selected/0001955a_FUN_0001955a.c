
LONG __fastcall FUN_0001955a(int param_1)

{
  LONG LVar1;
  uint uVar2;
  LONG *unaff_EDI;
  undefined4 uVar3;
  
  LVar1 = InterlockedDecrement(unaff_EDI);
  uVar2 = 0;
  if (((*(byte *)(param_1 + 0x110) & 4) != 0) &&
     (uVar2 = (uint)((*(byte *)(param_1 + 0x10c) & 1) != 0), *(int *)(param_1 + 0x1ac) != 0)) {
    uVar2 = uVar2 + 1;
  }
  if (LVar1 == uVar2 + 1) {
    KeSetEvent(*(undefined4 *)(param_1 + 0x1b4),0,0);
  }
  if (LVar1 == 1) {
    uVar3 = *(undefined4 *)(param_1 + 0x70);
  }
  else {
    if (LVar1 != 0) {
      return LVar1;
    }
    uVar3 = *(undefined4 *)(param_1 + 0x90);
  }
  KeSetEvent(uVar3,0,0);
  return LVar1;
}

