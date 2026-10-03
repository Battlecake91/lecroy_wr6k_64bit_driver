
undefined4 * __thiscall FUN_0001dd1e(void *this,undefined4 param_1,undefined4 param_2)

{
  int iVar1;
  
  iVar1 = (int)this + 0x10;
  *(int *)((int)this + 4) = iVar1;
  *(undefined1 *)((int)this + 8) = 0;
  *(undefined4 *)this = 0;
  *(undefined1 *)((int)this + 0xc) = 0;
  if (iVar1 != 0) {
    KeInitializeEvent(iVar1,param_1,param_2);
  }
  return this;
}

