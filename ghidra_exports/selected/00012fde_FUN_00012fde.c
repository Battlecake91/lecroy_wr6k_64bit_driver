
int __fastcall FUN_00012fde(int param_1)

{
  byte bVar1;
  undefined4 local_10;
  undefined4 local_c;
  byte local_5;
  
  *(undefined4 *)(param_1 + 0x10c6) = 1;
  WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x10a2),1);
  local_c = 0xffffffff;
  local_10 = 0xfffffc18;
  KeDelayExecutionThread(0,0,&local_10);
  bVar1 = READ_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x10a2));
  local_5 = bVar1 & 1;
  bVar1 = bVar1 & 1;
  if (local_5 == 0) {
    FUN_0001557c((void *)(param_1 + 0x10f2),0x4b0,'\0');
  }
  else {
    FUN_0001557c((void *)(param_1 + 0x10f2),300,'\0');
    *(undefined4 *)(param_1 + 0x10ee) = 7;
    WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x10ca),7);
    local_c = 0xffffffff;
    local_10 = 0xffffec78;
    KeDelayExecutionThread(0,0,&local_10);
    FUN_0001557c((void *)(param_1 + 0x10f2),300,'\0');
    *(undefined4 *)(param_1 + 0x10ee) = 3;
    WRITE_REGISTER_ULONG(*(undefined4 *)(param_1 + 0x10ca),3);
    bVar1 = local_5;
  }
  return (-(uint)(bVar1 != 0) & 0x3fffffff) + 0xc0000001;
}

