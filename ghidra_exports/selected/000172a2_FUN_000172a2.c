
undefined4 __thiscall FUN_000172a2(void *this,int *param_1)

{
  int iVar1;
  int local_8;
  
  local_8 = 0;
  if ((((param_1 != (int *)0x0) && (*param_1 != 0)) && (*(int *)((int)this + 0x104) != 0)) &&
     ((iVar1 = FUN_00018168((void *)((int)this + 0x100),*param_1,&local_8), iVar1 != 0 &&
      ((*(uint *)(iVar1 + 0xc) & 1) == 0)))) {
    if (local_8 == 0) {
      *(undefined4 *)((int)this + 0x104) = *(undefined4 *)(iVar1 + 0x3c);
    }
    else {
      *(undefined4 *)(local_8 + 0x3c) = *(undefined4 *)(iVar1 + 0x3c);
    }
    FUN_00017fd6();
    return 0;
  }
  return 0xc000000d;
}

