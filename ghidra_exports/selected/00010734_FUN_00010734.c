
int __thiscall FUN_00010734(void *this,byte param_1)

{
  if (((param_1 & 1) != 0) && (this != (void *)0x0)) {
    ExFreePool(this);
  }
  return (int)this;
}

