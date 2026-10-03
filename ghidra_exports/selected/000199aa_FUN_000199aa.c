
void FUN_000199aa(void *param_1,undefined4 param_2,undefined4 param_3,int param_4,int *param_5)

{
  int iVar1;
  
  iVar1 = *param_5;
  if ((param_4 != 0) && (*(code **)(param_4 + 8) != (code *)0x0)) {
    (**(code **)(param_4 + 8))(*(undefined4 *)((int)param_1 + 4),param_2,param_3,param_4,param_5);
  }
  if (-1 < iVar1) {
    FUN_00019e56(param_1,2,1,0,0);
  }
  *(undefined4 *)((int)param_1 + 0x1ac) = 0;
  if (param_4 != 0) {
    ExFreePool(param_4);
  }
  return;
}

