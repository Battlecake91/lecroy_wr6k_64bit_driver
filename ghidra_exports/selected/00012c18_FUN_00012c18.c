
undefined4 __thiscall FUN_00012c18(void *this,int param_1)

{
  void *this_00;
  int iVar1;
  int iVar2;
  
  this_00 = (void *)((int)this + 0x11ee);
  *(undefined4 *)(param_1 + 0x18) = 0xc0000002;
  iVar2 = FUN_00012386((int)this_00);
  iVar1 = *(int *)(*(int *)(param_1 + 0x60) + 4);
  if (iVar1 == 4) {
    **(int **)(param_1 + 0xc) = iVar2;
    *(undefined4 *)(param_1 + 0x18) = 0;
    *(undefined4 *)(param_1 + 0x1c) = 4;
    goto LAB_00012ca3;
  }
  if (DAT_0001cd08 == 0) {
    if (iVar1 == iVar2) {
      FUN_000123b4(this_00,*(undefined4 **)(param_1 + 0xc),iVar2);
    }
    else {
      if ((*(int *)(*(int *)(param_1 + 0x60) + 8) != 4) || (iVar2 = 0x10a, iVar1 != 0x10a)) {
        *(undefined4 *)(param_1 + 0x18) = 0xc0000206;
        goto LAB_00012c9f;
      }
      FUN_000124c2(this_00,**(int **)(param_1 + 0xc),*(int **)(param_1 + 0xc));
    }
    *(undefined4 *)(param_1 + 0x18) = 0;
    *(int *)(param_1 + 0x1c) = iVar2;
  }
  else {
    *(undefined4 *)(param_1 + 0x18) = 0xc00000a3;
LAB_00012c9f:
    *(undefined4 *)(param_1 + 0x1c) = 0;
  }
LAB_00012ca3:
  return *(undefined4 *)(param_1 + 0x18);
}

