
undefined4 __thiscall FUN_000107fe(void *this,undefined4 param_1)

{
  *(undefined4 *)((int)this + 0x24) = param_1;
  WRITE_REGISTER_ULONG(*(undefined4 *)this,param_1);
  return param_1;
}

