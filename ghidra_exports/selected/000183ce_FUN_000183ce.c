
undefined4 * __thiscall FUN_000183ce(void *this,byte param_1)

{
  FUN_000183b8(this);
  if (((param_1 & 1) != 0) && (this != (void *)0x0)) {
    ExFreePool(this);
  }
  return this;
}

