
undefined4 __fastcall FUN_000126ee(int param_1)

{
  *(undefined4 *)(param_1 + 0x224) = 3;
  WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x200),3);
  *(undefined4 *)(param_1 + 0x1d4) = 0xffffffff;
  WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x1b0),0xffffffff);
  READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x1b0));
  return 0;
}

