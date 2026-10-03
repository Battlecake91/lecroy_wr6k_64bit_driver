
undefined4 * __fastcall FUN_00016d60(undefined4 *param_1)

{
  int iVar1;
  
  param_1[3] = 0;
  *(undefined1 *)(param_1 + 6) = 0;
  *param_1 = &PTR_LAB_0001c9a4;
  iVar1 = FUN_00016cb0(param_1);
  if (iVar1 == 0) {
    *(undefined1 *)(param_1 + 6) = 1;
  }
  return param_1;
}

