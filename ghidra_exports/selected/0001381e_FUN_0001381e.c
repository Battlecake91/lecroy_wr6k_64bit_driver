
int __fastcall FUN_0001381e(int param_1)

{
  int iVar1;
  undefined **ppuVar2;
  uint uVar3;
  
  iVar1 = FUN_00012fde(param_1);
  if (iVar1 == 0) {
    FUN_0001260e(param_1);
    *(undefined4 *)(param_1 + 0xd24) = 0;
    WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0xd00),0);
    FUN_00011a30((undefined4 *)(param_1 + 0x368));
    FUN_00011a30((undefined4 *)(param_1 + 0x7bc));
    DAT_0001cd08 = 0;
    FUN_000126ee(param_1);
    DAT_0001ce18 = *(undefined4 *)(param_1 + 0xe90);
    ppuVar2 = FUN_00010a88();
    (**(code **)*ppuVar2)(FUN_00012eae,param_1);
    uVar3 = FUN_000174c8(param_1 + 0xed9);
    if ((char)uVar3 != '\0') {
      FUN_000157a6((void *)(param_1 + 0xea8),0x20);
      FUN_00010816((undefined4 *)(param_1 + 0x10fe));
    }
  }
  return iVar1;
}

