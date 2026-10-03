
undefined4 * __thiscall FUN_0001555a(void *this,byte param_1)

{
  *(undefined1 *)((int)this + 9) = 0;
  *(undefined ***)this = &PTR_FUN_0001c990;
  if ((param_1 & 1) != 0) {
    ExFreePool(this);
  }
  return this;
}

