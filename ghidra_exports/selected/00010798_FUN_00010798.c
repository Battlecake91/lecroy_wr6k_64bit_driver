
undefined4 __thiscall FUN_00010798(void *this,int param_1,undefined4 param_2)

{
  FUN_0001955a(param_1);
  *(undefined4 *)(*(int *)this + 0x18) = param_2;
  IofCompleteRequest();
  return param_2;
}

