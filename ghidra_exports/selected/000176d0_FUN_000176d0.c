
undefined4 __fastcall FUN_000176d0(int param_1)

{
  uint uVar1;
  
  uVar1 = READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x38));
  return CONCAT31((int3)((uVar1 & 0x8000) >> 8),(uVar1 & 0x8000) == 0x8000);
}

