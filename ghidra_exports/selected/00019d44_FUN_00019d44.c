
void FUN_00019d44(int param_1,undefined4 param_2,int param_3,int param_4,undefined4 *param_5)

{
  int iVar1;
  int iVar2;
  
  iVar2 = param_4;
  iVar1 = *(int *)(param_4 + 4);
  param_4 = iVar1;
  ExFreePool(iVar2);
  if (param_3 == 1) {
    *(uint *)(param_1 + 0x10c) = *(uint *)(param_1 + 0x10c) & 0xfffffff9;
  }
  PoStartNextPowerIrp(iVar1);
  FUN_00019c9c(&param_4,*param_5);
  return;
}

