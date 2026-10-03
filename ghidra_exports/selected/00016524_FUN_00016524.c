
undefined4 * __thiscall FUN_00016524(void *this,byte param_1)

{
  FUN_00016500(this);
  if (((param_1 & 1) != 0) && (this != (void *)0x0)) {
    ExFreePool(this);
  }
  return this;
}

