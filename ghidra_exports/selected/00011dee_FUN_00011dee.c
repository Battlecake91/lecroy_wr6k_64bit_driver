
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined1 FUN_00011dee(void)

{
  if ((_DAT_0001ce10 & 4) != 0) {
    _DAT_0001ce10 = _DAT_0001ce10 & 0xfffffffb;
    return 1;
  }
  return 0;
}

