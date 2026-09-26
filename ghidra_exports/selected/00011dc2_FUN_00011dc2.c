
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined1 FUN_00011dc2(void)

{
  if ((_DAT_0001ce10 & 1) != 0) {
    _DAT_0001ce10 = _DAT_0001ce10 & 0xfffffffe;
    return 1;
  }
  return 0;
}

