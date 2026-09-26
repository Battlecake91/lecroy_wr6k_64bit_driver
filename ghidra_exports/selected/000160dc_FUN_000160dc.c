
ushort __thiscall FUN_000160dc(void *this,int param_1,int *param_2)

{
  int iVar1;
  ushort uVar2;
  
  if (*(int *)((int)this + 0x182) == 0) {
    uVar2 = 4;
  }
  else {
    iVar1 = FUN_00018168((void *)(*(int *)((int)this + 0x182) + 0x100),*(int *)(param_1 + 1),
                         (int *)0x0);
    *(int *)(*(int *)((int)this + 0x182) + 0x100) = iVar1;
    iVar1 = *(int *)((int)*(void **)((int)this + 0x182) + 0x100);
    if ((iVar1 == 0) || ((uint)*(ushort *)(param_1 + 5) << 3 < *(uint *)(iVar1 + 4))) {
      uVar2 = 4;
    }
    else {
      iVar1 = FUN_00017478(*(void **)((int)this + 0x182),(uint)*(ushort *)(param_1 + 5),'\0');
      uVar2 = -(ushort)(iVar1 != 0) & 8;
      *param_2 = iVar1;
      *(undefined2 *)(param_2 + 1) = 2;
      *(ushort *)((int)param_2 + 6) = uVar2;
      FUN_00015a26(this,param_2,8);
    }
  }
  return uVar2;
}

