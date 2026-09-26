
undefined4 __thiscall FUN_00012988(void *this,int param_1)

{
  undefined4 *puVar1;
  undefined4 uVar2;
  int iVar3;
  uint uVar4;
  
  *(undefined4 *)(param_1 + 0x18) = 0xc0000002;
  if ((*(int *)(*(int *)(param_1 + 0x60) + 8) == 0xc) &&
     (*(int *)(*(int *)(param_1 + 0x60) + 4) == 0)) {
    puVar1 = *(undefined4 **)(param_1 + 0xc);
    IoGetCurrentProcess();
    uVar2 = FUN_00011b18((void *)((int)this + 0x111e),*puVar1,(uint)*(byte *)(param_1 + 0x20));
    *(undefined4 *)(param_1 + 0x18) = uVar2;
    IoGetCurrentProcess();
    uVar2 = FUN_00011b18((void *)((int)this + 0x112e),puVar1[1],(uint)*(byte *)(param_1 + 0x20));
    *(undefined4 *)(param_1 + 0x18) = uVar2;
    IoGetCurrentProcess();
    uVar2 = FUN_00011b18((undefined4 *)((int)this + 0x113e),puVar1[2],
                         (uint)*(byte *)(param_1 + 0x20));
    uVar4 = 2;
    *(undefined4 *)(param_1 + 0x18) = uVar2;
    iVar3 = IoGetCurrentProcess();
    FUN_0001232a((void *)((int)this + 0xe98),iVar3,uVar4);
    KeClearEvent(*(undefined4 *)((int)this + 0x111e));
    KeClearEvent(*(undefined4 *)((int)this + 0x112e));
    KeClearEvent(*(undefined4 *)((int)this + 0x113e));
    *(uint *)((int)this + 0x1126) = *(uint *)((int)this + 0x1126) | 1;
    *(uint *)((int)this + 0x1136) = *(uint *)((int)this + 0x1136) | 1;
    *(uint *)((int)this + 0x1146) = *(uint *)((int)this + 0x1146) | 1;
  }
  else {
    *(undefined4 *)(param_1 + 0x18) = 0xc0000206;
  }
  *(undefined4 *)(param_1 + 0x1c) = 0;
  return *(undefined4 *)(param_1 + 0x18);
}

