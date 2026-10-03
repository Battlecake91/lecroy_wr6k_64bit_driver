
void __fastcall FUN_00010598(int *param_1)

{
  if (((char)param_1[1] != '\0') && (*param_1 != 0)) {
    ExFreePool(*param_1);
  }
  *param_1 = 0;
  return;
}

