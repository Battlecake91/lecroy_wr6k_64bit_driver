
undefined4 __thiscall FUN_0001b98e(void *this,int param_1,undefined4 param_2)

{
  int iVar1;
  
  iVar1 = IoAttachDeviceToDeviceStack(*(undefined4 *)(param_1 + 4),param_2);
  *(undefined4 *)((int)this + 8) = 0;
  *(int *)((int)this + 4) = iVar1;
  if (iVar1 == 0) {
    *(undefined4 *)((int)this + 0xc) = 0;
    *(undefined4 *)this = 0xc000000e;
  }
  else {
    *(undefined4 *)((int)this + 0xc) = param_2;
  }
  return *(undefined4 *)this;
}

