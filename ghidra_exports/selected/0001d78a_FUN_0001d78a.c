
void __fastcall FUN_0001d78a(int *param_1)

{
  if (((char)param_1[6] != '\0') && (*param_1 != 0)) {
    ExFreePool(*param_1);
    *param_1 = 0;
  }
  return;
}

