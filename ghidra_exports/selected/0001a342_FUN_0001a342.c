
undefined4 FUN_0001a342(int *param_1,int param_2)

{
  int iVar1;
  bool bVar2;
  undefined4 *puVar3;
  undefined4 uVar4;
  int iVar5;
  undefined4 *puVar6;
  
  iVar1 = param_2;
  iVar5 = *(int *)(param_2 + 0x18);
  if ((-1 < iVar5) &&
     ((param_1[0x6a] = *(int *)(*(int *)(param_2 + 0x60) + 0xc),
      (*(byte *)(param_1 + 0x44) & 0x10) != 0 || (bVar2 = FUN_0001a040(param_1 + 0x2b), !bVar2)))) {
    puVar3 = (undefined4 *)ExAllocatePoolWithTag(0,8,0x206d6457);
    puVar6 = (undefined4 *)0x0;
    if (puVar3 != (undefined4 *)0x0) {
      puVar3[1] = 0;
      puVar6 = puVar3;
    }
    if (puVar6 == (undefined4 *)0x0) {
      param_2 = -0x3fffff66;
      iVar5 = param_2;
    }
    else {
      *puVar6 = param_1;
      puVar6[1] = param_2;
      uVar4 = (**(code **)(*param_1 + 0x124))(1);
      iVar5 = PoRequestPowerIrp(param_1[0xd],2,uVar4,&LAB_0001a07e,puVar6,0);
      if (-1 < iVar5) {
        return 0xc0000016;
      }
      ExFreePool(puVar6);
    }
  }
  PoStartNextPowerIrp(iVar1);
  *(int *)(iVar1 + 0x18) = iVar5;
  FUN_0001955a((int)param_1);
  return 0;
}

