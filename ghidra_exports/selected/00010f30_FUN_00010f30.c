
undefined4 __thiscall FUN_00010f30(void *this,int param_1)

{
  int iVar1;
  undefined4 uVar2;
  void *local_c;
  void *local_8;
  
  local_c = this;
  local_8 = this;
  FUN_00013768((undefined4 *)((int)this + 0x1e0));
  local_c = (void *)(*(int *)((int)this + 4) + 0x60);
  local_8 = (void *)((uint)local_8 & 0xffff0000);
  if (*(int *)(param_1 + 0x60) == 0) {
    iVar1 = 0;
  }
  else {
    iVar1 = *(int *)(*(int *)(param_1 + 0x60) + 0x18);
  }
  FUN_0001bb4e(&local_c,(int)this,iVar1);
  uVar2 = FUN_00010798(&param_1,(int)this,0);
  if (local_c != (void *)0x0) {
    FUN_00010598((int *)&local_c);
  }
  return uVar2;
}

