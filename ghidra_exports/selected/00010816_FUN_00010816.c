
undefined1 __fastcall FUN_00010816(undefined4 *param_1)

{
  if (param_1[2] == 1) {
    KeSetEvent(*param_1,0,0);
    return 1;
  }
  return 0;
}

