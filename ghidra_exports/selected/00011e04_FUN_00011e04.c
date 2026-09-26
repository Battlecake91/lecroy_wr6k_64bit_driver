
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined1 FUN_00011e04(void)

{
  if ((_DAT_0001ce10 & 0x10) != 0) {
    _DAT_0001ce10 = _DAT_0001ce10 & 0xffffffef;
    return 1;
  }
  return 0;
}

