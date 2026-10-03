
undefined4 * __thiscall FUN_0001dcd4(void *this,undefined4 param_1)

{
  int iVar1;
  
  *(undefined1 *)((int)this + 8) = 0;
  *(undefined4 *)this = 0;
  iVar1 = (int)this + 0xc;
  *(int *)((int)this + 4) = iVar1;
  if (iVar1 != 0) {
    KeInitializeMutex(iVar1,param_1);
  }
  return this;
}

