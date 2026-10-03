
bool FUN_0001896e(void)

{
  int iVar1;
  bool bVar2;
  undefined4 local_20;
  undefined4 local_1c;
  undefined4 local_18;
  int local_14 [3];
  undefined4 local_8;
  
  FUN_0001be34(local_14,L"\\Device\\DebugMessageDevice",0x1f01ff);
  if (local_14[0] < 0) {
    FUN_000105f2((int)local_14);
    bVar2 = false;
  }
  else {
    iVar1 = FUN_0001bf5a(local_14,0x222003,0,0,&local_20,0xc,1,&local_8);
    bVar2 = -1 < iVar1;
    if (bVar2) {
      DAT_0001d250 = local_20;
      DAT_0001d258 = local_18;
      DAT_0001d254 = local_1c;
    }
    FUN_000105f2((int)local_14);
  }
  return bVar2;
}

