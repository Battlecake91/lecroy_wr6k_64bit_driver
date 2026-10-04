
void __thiscall FUN_00010d06(int param_1,int param_2)

{
  undefined4 *puVar1;
  int iVar2;
  undefined4 *puVar3;
  undefined4 *puVar4;
  
  PoStartNextPowerIrp(param_2);
  puVar1 = *(undefined4 **)(param_2 + 0x60);
  puVar3 = puVar1;
  puVar4 = puVar1 + -9;
  for (iVar2 = 7; iVar2 != 0; iVar2 = iVar2 + -1) {
    *puVar4 = *puVar3;
    puVar3 = puVar3 + 1;
    puVar4 = puVar4 + 1;
  }
  *(undefined1 *)((int)puVar1 + -0x21) = 0;
  FUN_000107c2((void *)(param_1 + 0x14a5),param_1,param_2);
  return;
}

