
void __fastcall FUN_00019ccc(undefined4 *param_1)

{
  int iVar1;
  
  *param_1 = &PTR_LAB_0001ca88;
  while( true ) {
    iVar1 = FUN_00019c26(param_1 + 0x74);
    if (iVar1 == 0) break;
    if (*(int *)(iVar1 + 8) != 0) {
      RtlFreeUnicodeString(iVar1 + 4);
    }
    ExFreePool(iVar1);
  }
  FUN_00018870(param_1 + 0x6c);
  FUN_00018870(param_1 + 0x23);
  FUN_00018870(param_1 + 0x1b);
  if (param_1[0x11] != 0) {
    FUN_00011914(param_1 + 0x10);
  }
  FUN_0001d476(param_1);
  return;
}

