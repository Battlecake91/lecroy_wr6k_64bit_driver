
undefined4 * __thiscall FUN_000157c4(void *this,undefined4 param_1)

{
  *(undefined1 *)((int)this + 0x10) = 0;
  *(undefined4 *)((int)this + 8) = 0;
  *(int *)((int)this + 0xc) = (int)this + 0x18;
  *(undefined ***)this = &PTR_LAB_0001c994;
  KeInitializeTimerEx((int)this + 0x18,param_1);
  return this;
}

