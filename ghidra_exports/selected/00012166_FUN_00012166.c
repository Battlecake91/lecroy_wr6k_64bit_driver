
void __fastcall FUN_00012166(undefined4 *param_1)

{
  if (param_1[4] != 0) {
    ExFreePool(param_1[4]);
  }
  param_1[4] = 0;
  param_1[1] = 0;
  *param_1 = 0;
  return;
}

