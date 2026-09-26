
void __thiscall FUN_0001340c(void *this,undefined4 param_1,undefined4 param_2,undefined4 param_3)

{
  *(uint *)((int)this + 0x10) = *(uint *)((int)this + 0x10) | 0x1ff000;
  *(undefined4 *)((int)this + 4) = param_1;
  *(undefined4 *)((int)this + 8) = param_2;
  *(undefined4 *)((int)this + 0xc) = param_3;
  *(undefined1 *)this = 1;
  FUN_0001236e(this);
  return;
}

