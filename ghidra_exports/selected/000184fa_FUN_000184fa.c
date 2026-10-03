
undefined4 * __thiscall FUN_000184fa(void *this,byte param_1)

{
  (**(code **)((int)this + 4))();
  DAT_0001ce48 = *(undefined4 *)this;
  if ((param_1 & 1) != 0) {
    FUN_000103b6((int)this);
  }
  return this;
}

