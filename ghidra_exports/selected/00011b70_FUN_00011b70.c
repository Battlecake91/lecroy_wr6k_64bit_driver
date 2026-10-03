
undefined4 __thiscall FUN_00011b70(void *this,int param_1)

{
  undefined4 uVar1;
  void *local_c;
  void *local_8;
  
  uVar1 = 0;
  if (*(int *)(*(int *)(param_1 + 0x60) + 0xc) == 5) {
    local_c = this;
    local_8 = this;
    if (*(int *)((int)this + 0x1136) == 1) {
      FUN_00010816((undefined4 *)((int)this + 0x112e));
    }
    if (*(int *)((int)this + 0x1146) == 1) {
      KeResetEvent(*(undefined4 *)((int)this + 0x113e));
      local_8 = (void *)0xffffffff;
      local_c = (void *)0xee1e5d00;
      uVar1 = KeWaitForSingleObject(*(undefined4 *)((int)this + 0x113e),6,1,0,&local_c);
    }
  }
  return uVar1;
}

