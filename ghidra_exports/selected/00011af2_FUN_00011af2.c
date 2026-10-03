
void __fastcall FUN_00011af2(undefined4 *param_1)

{
  while (0 < (int)param_1[1]) {
    param_1[1] = param_1[1] + -1;
    ObfDereferenceObject();
  }
  *param_1 = 0;
  param_1[2] = 0;
  param_1[3] = 0;
  param_1[1] = 0;
  return;
}

