
uint entry(int param_1,ushort *param_2)

{
  int iVar1;
  uint uVar2;
  undefined1 local_2c [4];
  int local_28;
  int local_24;
  undefined1 local_18 [4];
  int local_14;
  int local_10;
  
  uVar2 = FUN_000185de();
  if ((uVar2 & 0xc0000000) == 0xc0000000) {
    FUN_00018542();
  }
  else {
    FUN_0001dc24(local_2c,2,u_Class_0001df2a,'\0',0,0x20019,0x40);
    FUN_0001dc24(local_18,1,u_Class_0001df36,'\0',0,0x20019,0x40);
    DAT_0001ce08 = local_24 < 0 && -1 < local_10;
    uVar2 = FUN_0001de46(param_1,param_2);
    iVar1 = local_14;
    if (local_14 != 0) {
      local_14 = 0;
      ZwClose(iVar1);
    }
    iVar1 = local_28;
    if (local_28 != 0) {
      local_28 = 0;
      ZwClose(iVar1);
    }
  }
  return uVar2;
}

