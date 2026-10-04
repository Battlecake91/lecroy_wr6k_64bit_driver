
void __fastcall FUN_00010eaf(int param_1)

{
  uint *puVar1;
  
  if (DAT_0001ce0c == 0) {
    *(undefined4 *)(param_1 + 0x1452) = 3;
    puVar1 = FUN_00010750((uint *)(param_1 + 0x1436));
    FUN_00010750(puVar1);
  }
  else {
    DAT_0001ce0c = DAT_0001ce0c + -1;
  }
  if (DAT_0001ce0c == 0) {
    *(undefined4 *)(param_1 + 0xf04) = 0;
    WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0xee0),0);
    DAT_0001ce14 = 0xffffffff;
    DAT_0001ce44 = 0xffffffff;
    WRITE_REGISTER_ULONG(DAT_0001ce20,0xffffffff);
  }
  FUN_00010798(&stack0x00000004,param_1,0);
  return;
}

