
void __fastcall FUN_0001260e(int param_1)

{
  uint uVar1;
  uint uVar2;
  uint uVar3;
  
  uVar1 = READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x1b0));
  if ((uVar1 & 1) != 0) {
    *(undefined4 *)(param_1 + 0x1fc) = 0;
    WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x1d8),0);
  }
  if ((uVar1 & 2) != 0) {
    uVar2 = READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x160));
    uVar3 = (uint)((uVar2 & 0x400) != 0);
    if ((uVar2 & 0x800) != 0) {
      uVar3 = uVar3 | 2;
    }
    if ((uVar2 & 0x1000) != 0) {
      uVar3 = uVar3 | 4;
    }
    if ((uVar2 & 0x2000) != 0) {
      uVar3 = uVar3 | 8;
    }
    if ((uVar2 & 0x4000) != 0) {
      uVar3 = uVar3 | 0x10;
    }
    if (uVar3 != 0) {
      FUN_000107fe((void *)(param_1 + 0x228),uVar3);
    }
    FUN_000107fe((undefined4 *)(param_1 + 0x160),uVar2);
  }
  if ((uVar1 & 4) != 0) {
    *(undefined4 *)(param_1 + 0x224) = 1;
    WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x200),1);
  }
  if ((uVar1 & 8) != 0) {
    *(undefined4 *)(param_1 + 0x224) = 2;
    WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x200),2);
  }
  FUN_000107fe((undefined4 *)(param_1 + 0x1b0),uVar1);
  return;
}

