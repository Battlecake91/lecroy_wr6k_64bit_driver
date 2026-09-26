
undefined4 FUN_00011e46(void)

{
  uint in_EAX;
  undefined4 uVar1;
  
  if (DAT_0001cd08 != -1) {
    DAT_0001ce14 = DAT_0001ce18;
    DAT_0001ce44 = DAT_0001ce18;
    uVar1 = WRITE_REGISTER_ULONG(DAT_0001ce20,DAT_0001ce18);
    return CONCAT31((int3)((uint)uVar1 >> 8),1);
  }
  return in_EAX & 0xffffff00;
}

