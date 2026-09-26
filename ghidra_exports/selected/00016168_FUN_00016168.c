
undefined4 __thiscall FUN_00016168(void *this,int param_1,int param_2)

{
  undefined4 uVar1;
  
  FUN_000176e6((void *)((int)this + 0x31),(ushort *)(param_1 + 6),*(ushort *)(param_1 + 2) + 2,1);
  uVar1 = 0;
  if (param_2 == 0) {
    uVar1 = 8;
  }
  else {
    *(undefined1 *)((int)this + 0x24) = 1;
  }
  return uVar1;
}

