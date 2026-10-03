
void FUN_00019d84(int param_1,undefined4 param_2,undefined4 param_3,int param_4,int *param_5)

{
  int iVar1;
  undefined4 *puVar2;
  int iVar3;
  undefined4 *puVar4;
  undefined4 *puVar5;
  
  iVar3 = param_4;
  iVar1 = *(int *)(param_4 + 4);
  param_4 = iVar1;
  ExFreePool(iVar3);
  PoStartNextPowerIrp(iVar1);
  if (*param_5 < 0) {
    FUN_00019c9c(&param_4,*param_5);
  }
  else {
    puVar2 = *(undefined4 **)(iVar1 + 0x60);
    puVar4 = puVar2;
    puVar5 = puVar2 + -9;
    for (iVar3 = 7; iVar3 != 0; iVar3 = iVar3 + -1) {
      *puVar5 = *puVar4;
      puVar4 = puVar4 + 1;
      puVar5 = puVar5 + 1;
    }
    *(undefined1 *)((int)puVar2 + -0x21) = 0;
    FUN_000107c2(*(void **)(param_1 + 0x30),param_1,iVar1);
  }
  return;
}

