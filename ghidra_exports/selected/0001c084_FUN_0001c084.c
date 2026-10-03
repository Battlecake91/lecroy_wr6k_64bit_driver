
void __fastcall FUN_0001c084(int *param_1)

{
  if (((char)param_1[3] != '\0') && (param_1[1] != 0)) {
    ZwClose(*param_1);
    *(undefined1 *)(param_1 + 3) = 0;
  }
  FUN_00011914(param_1);
  return;
}

