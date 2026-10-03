
undefined4 __thiscall FUN_00013954(void *this,int param_1)

{
  byte bVar1;
  byte *pbVar2;
  int iVar3;
  undefined4 uVar4;
  int iVar5;
  
  pbVar2 = *(byte **)(param_1 + 0xc);
  if (pbVar2 == (byte *)0x0) {
    *(undefined4 *)(param_1 + 0x1c) = 0;
    *(undefined4 *)(param_1 + 0x18) = 0xc000000d;
  }
  iVar3 = *(int *)(param_1 + 0x60);
  if ((*(int *)(iVar3 + 8) == 5) && (*(int *)(iVar3 + 4) == 4)) {
    bVar1 = *pbVar2;
    if ((bVar1 != 0) && (DAT_0001cd08 == -1)) {
      *(undefined4 *)(param_1 + 0x1c) = 0;
      *(undefined4 *)(param_1 + 0x18) = 0xc00000a3;
      return 0xc00000a3;
    }
    if (2 < bVar1) {
      return 0xc000000d;
    }
    iVar5 = *(int *)(pbVar2 + 1);
    iVar3 = (**(code **)(*(int *)this + 0x1c))(CONCAT31((int3)((uint)iVar3 >> 8),bVar1));
    uVar4 = READ_REGISTER_ULONG(*(int *)(iVar3 + 0x10) + iVar5);
    (**(code **)(*(int *)this + 0x1c))(*pbVar2);
    **(undefined4 **)(param_1 + 0xc) = uVar4;
  }
  else {
    if ((*(int *)(iVar3 + 8) != 4) || (*(int *)(iVar3 + 4) != 4)) {
      return 0xc0000206;
    }
    iVar3 = *(int *)pbVar2;
    iVar5 = (**(code **)(*(int *)this + 0x1c))(0);
    uVar4 = READ_REGISTER_ULONG(*(int *)(iVar5 + 0x10) + iVar3);
    (**(code **)(*(int *)this + 0x1c))(0);
    **(undefined4 **)(param_1 + 0xc) = uVar4;
  }
  *(undefined4 *)(param_1 + 0x18) = 0;
  *(undefined4 *)(param_1 + 0x1c) = 4;
  return 0;
}

