
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 __fastcall FUN_00014142(int param_1)

{
  void *this;
  int local_8;
  
  DAT_0001cd08 = 0xffffffff;
  this = (void *)(param_1 + 0x114e);
  _DAT_0001ce10 = 0;
  DAT_0001ce14 = 0;
  DAT_0001ce18 = 0;
  local_8 = param_1;
  FUN_00017a98(this,0,1,0);
  FUN_00013f3c(this,param_1 + 0x10fe,&local_8);
  FUN_00013f3c(this,param_1 + 0x110e,&local_8);
  FUN_00013f3c(this,param_1 + 0x111e,&local_8);
  FUN_00013f3c(this,param_1 + 0x112e,&local_8);
  FUN_00013f3c(this,param_1 + 0x113e,&local_8);
  return 0;
}

