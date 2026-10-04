
undefined4 FUN_00010c18(int param_1)

{
  *(undefined4 *)(param_1 + 0x18) = 0xc0000002;
  IofCompleteRequest();
  return 0xc0000002;
}

