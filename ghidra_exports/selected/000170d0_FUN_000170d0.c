
undefined4 * __thiscall FUN_000170d0(undefined4 *param_1,byte param_2)

{
  *param_1 = &PTR_FUN_0001c9a8;
  if ((param_2 & 1) != 0) {
    ExFreePool(param_1);
  }
  return param_1;
}

