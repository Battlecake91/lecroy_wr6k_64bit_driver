
undefined1 __thiscall FUN_0001941a(void *this,char param_1)

{
  undefined1 uVar1;
  
  if (param_1 != '\0') {
    FUN_000193b8();
  }
  uVar1 = *(undefined1 *)(*(int *)this + 0x24);
  if (param_1 != '\0') {
    IoReleaseCancelSpinLock(DAT_0001d26c);
  }
  return uVar1;
}

