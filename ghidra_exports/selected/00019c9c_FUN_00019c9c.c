
undefined4 __thiscall FUN_00019c9c(void *this,undefined4 param_1)

{
  FUN_0001955a(*(int *)(*(int *)(*(int *)(*(int *)this + 0x60) + 0x14) + 0x28));
  *(undefined4 *)(*(int *)this + 0x18) = param_1;
  IofCompleteRequest();
  return param_1;
}

