
int __fastcall FUN_00019c26(int *param_1)

{
  int iVar1;
  
  (**(code **)*param_1)();
  iVar1 = FUN_0001c06a(param_1 + 1);
  (**(code **)(*param_1 + 4))();
  return iVar1;
}

