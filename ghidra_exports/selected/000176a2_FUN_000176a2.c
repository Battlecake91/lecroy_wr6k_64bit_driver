
bool __fastcall FUN_000176a2(int param_1)

{
  short sVar1;
  short *unaff_retaddr;
  
  sVar1 = READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0xd8));
  *unaff_retaddr = sVar1;
  if (sVar1 != 0) {
    FUN_000107fe((undefined4 *)(param_1 + 0xd8),0);
  }
  return sVar1 != 0;
}

