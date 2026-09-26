
undefined4 __thiscall FUN_00012b34(void *this,int param_1)

{
  undefined4 uVar1;
  int iVar2;
  uint uVar3;
  
  *(undefined4 *)(param_1 + 0x18) = 0xc0000002;
  if ((*(int *)(*(int *)(param_1 + 0x60) + 8) == 4) && (*(int *)(*(int *)(param_1 + 0x60) + 4) == 0)
     ) {
    IoGetCurrentProcess();
    uVar1 = FUN_00011b18((undefined4 *)((int)this + 0x110e),**(undefined4 **)(param_1 + 0xc),
                         (uint)*(byte *)(param_1 + 0x20));
    uVar3 = 2;
    *(undefined4 *)(param_1 + 0x18) = uVar1;
    iVar2 = IoGetCurrentProcess();
    FUN_0001232a((void *)((int)this + 0xe98),iVar2,uVar3);
    KeClearEvent(*(undefined4 *)((int)this + 0x110e));
    *(uint *)((int)this + 0x1116) = *(uint *)((int)this + 0x1116) | 1;
  }
  else {
    *(undefined4 *)(param_1 + 0x18) = 0xc0000206;
  }
  *(undefined4 *)(param_1 + 0x1c) = 0;
  return *(undefined4 *)(param_1 + 0x18);
}

