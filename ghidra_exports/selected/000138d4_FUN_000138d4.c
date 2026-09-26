
undefined4 __fastcall FUN_000138d4(int param_1)

{
  undefined **ppuVar1;
  
  *(undefined4 *)(param_1 + 0xe90) = DAT_0001ce18;
  *(undefined4 *)(param_1 + 0x1ac) = 0xffffffff;
  WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x188),0xffffffff);
  DAT_0001ce18 = 0;
  ppuVar1 = FUN_00010a88();
  (**(code **)*ppuVar1)(FUN_00012eae,param_1);
  return 0;
}

