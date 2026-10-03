
void __fastcall FUN_000105cc(undefined2 *param_1)

{
  if (*(char *)(param_1 + 4) != '\0') {
    if (*(int *)(param_1 + 2) != 0) {
      ExFreePool(*(int *)(param_1 + 2));
    }
    *(undefined1 *)(param_1 + 4) = 0;
  }
  *param_1 = 0;
  *(undefined4 *)(param_1 + 2) = 0;
  return;
}

