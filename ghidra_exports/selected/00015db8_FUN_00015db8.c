
undefined4 __thiscall FUN_00015db8(void *this,int param_1,undefined4 param_2,int param_3)

{
  uint uVar1;
  undefined4 uVar2;
  
  uVar1 = FUN_000176e6((void *)((int)this + 0x31),(ushort *)(param_1 + 6),
                       *(ushort *)(param_1 + 2) + 2,param_3);
  if ((char)uVar1 == '\0') {
    uVar2 = 8;
  }
  else {
    uVar2 = 0;
    *(undefined1 *)((int)this + 0x24) = 1;
  }
  return uVar2;
}

