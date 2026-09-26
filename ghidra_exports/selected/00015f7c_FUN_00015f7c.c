
undefined2 __thiscall FUN_00015f7c(void *this,char *param_1)

{
  undefined2 uVar1;
  void *this_00;
  
  uVar1 = 0x10;
  if (param_1 == (char *)0x0) {
    uVar1 = 8;
  }
  else {
    this_00 = *(void **)((int)this + 0x166);
    if ((this_00 == (void *)0x0) || (*(void **)((int)this + 0x16e) == (void *)0x0)) {
      uVar1 = 8;
    }
    else {
      if ((*param_1 == '\0') || (this_00 = *(void **)((int)this + 0x16e), *param_1 == '\x01')) {
        FUN_000107fe(this_00,(uint)*(ushort *)(param_1 + 1));
        uVar1 = 0;
      }
      FUN_00015a88(this,uVar1);
    }
  }
  return uVar1;
}

