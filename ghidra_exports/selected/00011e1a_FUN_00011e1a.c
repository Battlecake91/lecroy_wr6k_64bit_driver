
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined1 FUN_00011e1a(void)

{
  if ((_DAT_0001ce10 & 0x20) != 0) {
    _DAT_0001ce10 = _DAT_0001ce10 & 0xffffffdf;
    return 1;
  }
  return 0;
}

