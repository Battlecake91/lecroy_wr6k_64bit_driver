
undefined4 __fastcall FUN_00017560(int param_1)

{
  short sVar1;
  
  sVar1 = READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x10));
  if ((-1 < sVar1) && ((char)sVar1 == '\0')) {
    return 1;
  }
  return 0;
}

