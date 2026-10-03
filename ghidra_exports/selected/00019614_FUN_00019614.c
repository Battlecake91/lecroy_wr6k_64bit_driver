
undefined4 __thiscall FUN_00019614(void *this,int param_1,int param_2)

{
  byte bVar1;
  undefined4 uVar2;
  void **ppvVar3;
  void *local_c;
  void *local_8;
  
  local_c = this;
  local_8 = this;
  bVar1 = KeGetCurrentIrql();
  if (bVar1 < 2) {
    if (param_1 == 0 && param_2 == 0) {
      ppvVar3 = (void **)0x0;
    }
    else {
      local_c = (void *)-param_1;
      local_8 = (void *)-(param_2 + (uint)(param_1 != 0));
      ppvVar3 = &local_c;
    }
    uVar2 = KeWaitForSingleObject(*(undefined4 *)((int)this + 0x70),0,0,0,ppvVar3);
  }
  else {
    uVar2 = 0xc0000001;
  }
  return uVar2;
}

