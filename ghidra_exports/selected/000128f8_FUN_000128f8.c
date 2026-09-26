
undefined4 __thiscall FUN_000128f8(void *this,int param_1)

{
  undefined4 *this_00;
  ushort uVar1;
  undefined4 uVar2;
  int iVar3;
  undefined2 extraout_var;
  uint uVar4;
  
  *(undefined4 *)(param_1 + 0x18) = 0xc0000002;
  if ((*(int *)(*(int *)(param_1 + 0x60) + 8) == 4) && (*(int *)(*(int *)(param_1 + 0x60) + 4) == 0)
     ) {
    this_00 = (undefined4 *)((int)this + 0x10fe);
    IoGetCurrentProcess();
    uVar2 = FUN_00011b18(this_00,**(undefined4 **)(param_1 + 0xc),(uint)*(byte *)(param_1 + 0x20));
    uVar4 = 2;
    *(undefined4 *)(param_1 + 0x18) = uVar2;
    iVar3 = IoGetCurrentProcess();
    FUN_0001232a((void *)((int)this + 0xe98),iVar3,uVar4);
    KeClearEvent(*this_00);
    *(uint *)((int)this + 0x1106) = *(uint *)((int)this + 0x1106) | 1;
    uVar1 = FUN_000157b8((int)this + 0xea8);
    if (CONCAT22(extraout_var,uVar1) != 0) {
      FUN_00010816(this_00);
    }
  }
  else {
    *(undefined4 *)(param_1 + 0x18) = 0xc0000206;
  }
  *(undefined4 *)(param_1 + 0x1c) = 0;
  return *(undefined4 *)(param_1 + 0x18);
}

