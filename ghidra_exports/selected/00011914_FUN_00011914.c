
void __fastcall FUN_00011914(int *param_1)

{
  if (param_1[1] != 0) {
    if (*param_1 == -1) {
      ExFreePool(param_1[1]);
      *param_1 = 0;
    }
    if ((char)param_1[2] != '\0') {
      ObfDereferenceObject();
      *(undefined1 *)(param_1 + 2) = 0;
    }
  }
  param_1[1] = 0;
  return;
}

