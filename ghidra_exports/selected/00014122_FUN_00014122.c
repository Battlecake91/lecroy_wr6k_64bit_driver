
undefined4 * __thiscall FUN_00014122(void *this,byte param_1)

{
  FUN_000134f6(this);
  if (((param_1 & 1) != 0) && (this != (void *)0x0)) {
    ExFreePool(this);
  }
  return this;
}

