
void FUN_00015a08(int param_1,undefined4 *param_2,undefined2 param_3)

{
  if ((param_2 != (undefined4 *)0x0) && (param_1 != 0)) {
    *param_2 = 0;
    *(undefined2 *)(param_2 + 1) = param_3;
  }
  return;
}

