
undefined4 __thiscall FUN_00017eac(void *this,undefined4 param_1,undefined4 *param_2)

{
  FUN_00017dac(this,*(int *)((int)this + 0xc) + 1,(undefined1 *)&param_1);
  if (param_2 != (undefined4 *)0x0) {
    if (*(int *)((int)this + 0x14) < 0) {
      *param_2 = 0xffffffff;
    }
    else {
      *param_2 = *(undefined4 *)((int)this + 0xc);
    }
  }
  return *(undefined4 *)((int)this + 0x14);
}

