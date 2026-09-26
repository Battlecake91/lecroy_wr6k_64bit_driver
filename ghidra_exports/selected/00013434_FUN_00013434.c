
undefined4 * __fastcall FUN_00013434(undefined4 *param_1)

{
  FUN_00017a16(param_1);
  FUN_00017a16(param_1 + 7);
  FUN_00019362(param_1 + 0xe,"CKeRegisterList",6,0,5,0,0);
  *(undefined1 *)(param_1 + 0x18) = 0;
  *(undefined1 *)((int)param_1 + 0x61) = 1;
  param_1[0x19] = 0;
  param_1[0xf] = 2;
  FUN_00018a1c(param_1 + 0xe,1,"CKeRegisterList()\n");
  FUN_00017a98(param_1,(uint)*(byte *)(param_1 + 0x18),(uint)*(byte *)((int)param_1 + 0x61),0);
  FUN_000120fa(param_1 + 7,(uint)*(byte *)(param_1 + 0x18),(uint)*(byte *)((int)param_1 + 0x61),0);
  return param_1;
}

