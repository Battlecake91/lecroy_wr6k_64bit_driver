
int __thiscall
FUN_0001bf5a(void *this,undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
            undefined4 param_5,undefined4 param_6,undefined4 *param_7)

{
  int iVar1;
  int local_2c;
  undefined4 local_28;
  int local_c;
  undefined4 local_8;
  
  FUN_0001dd1e(&local_2c,0,0);
  iVar1 = IoBuildDeviceIoControlRequest
                    (param_1,*(undefined4 *)((int)this + 4),param_2,param_3,param_4,param_5,param_6,
                     local_28,&local_c);
  if (iVar1 == 0) {
    iVar1 = -0x3fffff66;
  }
  else {
    iVar1 = IofCallDriver();
    if (iVar1 == 0x103) {
      KeWaitForSingleObject(local_28,0,0,1,0);
      iVar1 = local_c;
    }
    *param_7 = local_8;
  }
  FUN_00018870(&local_2c);
  return iVar1;
}

