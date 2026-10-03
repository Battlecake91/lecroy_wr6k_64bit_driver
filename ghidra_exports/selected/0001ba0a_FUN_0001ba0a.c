
undefined4 __thiscall FUN_0001ba0a(void *this,int param_1,undefined1 param_2,undefined4 *param_3)

{
  int iVar1;
  int local_3c;
  undefined4 local_38;
  int *local_1c;
  undefined4 local_18;
  undefined4 local_14;
  undefined4 local_10;
  undefined4 local_c;
  undefined1 local_8;
  
  FUN_0001dd1e(&local_3c,0,0);
  iVar1 = param_1;
  local_1c = &local_3c;
  local_18 = *(undefined4 *)(*(int *)(param_1 + 0x60) + -8);
  local_14 = *(undefined4 *)(*(int *)(param_1 + 0x60) + -4);
  local_8 = param_2;
  FUN_000193d0(&param_1,0x1be72,&local_1c,'\x01','\x01','\x01');
  PoCallDriver(*(undefined4 *)((int)this + 4),iVar1);
  KeWaitForSingleObject(local_38,0,0,1,0);
  if (param_3 != (undefined4 *)0x0) {
    *param_3 = local_c;
  }
  FUN_00018870(&local_3c);
  return local_10;
}

