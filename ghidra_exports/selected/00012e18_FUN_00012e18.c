
undefined4 __thiscall FUN_00012e18(void *this,int param_1)

{
  int iVar1;
  int iVar2;
  
  iVar1 = 0;
  iVar2 = *(int *)((int)this + 0x104);
  while (iVar2 != 0) {
    if (*(int *)(iVar2 + 0x34) == param_1) {
      if (iVar1 == 0) {
        *(undefined4 *)((int)this + 0x104) = *(undefined4 *)(iVar2 + 0x3c);
      }
      else {
        *(undefined4 *)(iVar1 + 0x3c) = *(undefined4 *)(iVar2 + 0x3c);
      }
      iVar2 = *(int *)(iVar2 + 0x3c);
      FUN_00017fd6();
    }
    else {
      iVar1 = iVar2;
      iVar2 = *(int *)(iVar2 + 0x3c);
    }
  }
  return 0;
}

