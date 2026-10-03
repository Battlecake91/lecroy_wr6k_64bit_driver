
undefined4 FUN_000197ee(int param_1,undefined4 param_2)

{
  *(uint *)(param_1 + 0x10c) = *(uint *)(param_1 + 0x10c) & 0xfffffffe;
  PoStartNextPowerIrp(param_2);
  FUN_0001955a(param_1);
  return 0;
}

