
undefined4 * __thiscall
FUN_0001b9c6(void *this,undefined4 param_1,undefined4 param_2,undefined4 *param_3)

{
  FUN_0001be20(this,param_1);
  if (*(int *)((int)this + 4) == 0) {
    *(undefined4 *)((int)this + 0xc) = 0;
    *(undefined4 *)this = 0xc000000e;
  }
  else {
    *(undefined4 *)((int)this + 0xc) = param_2;
  }
  *param_3 = *(undefined4 *)this;
  return this;
}

