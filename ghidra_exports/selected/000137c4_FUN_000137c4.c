
undefined4 __fastcall FUN_000137c4(int param_1)

{
  undefined **ppuVar1;
  
  FUN_0001260e(param_1);
  FUN_000126ee(param_1);
  *(undefined4 *)(param_1 + 0xd24) = 0;
  WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0xd00),0);
  *(undefined4 *)(param_1 + 0x1ac) = 0xffffffff;
  WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x188),0xffffffff);
  DAT_0001ce18 = 0;
  ppuVar1 = FUN_00010a88();
  (**(code **)*ppuVar1)(FUN_00012eae,param_1);
  DAT_0001cd08 = 0xffffffff;
  return 0;
}

