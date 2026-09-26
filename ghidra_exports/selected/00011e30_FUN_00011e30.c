
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined1 FUN_00011e30(void)

{
  if ((_DAT_0001ce10 & 8) != 0) {
    _DAT_0001ce10 = _DAT_0001ce10 & 0xfffffff7;
    return 1;
  }
  return 0;
}

