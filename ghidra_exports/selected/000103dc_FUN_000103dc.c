
void __fastcall FUN_000103dc(undefined4 *param_1)

{
  *param_1 = &PTR_FUN_0001c4c4;
  if (*(int *)(DAT_0001cdf8 + 0xc) != 0) {
    ExFreePool(*(int *)(DAT_0001cdf8 + 0xc));
  }
  if (DAT_0001ce04 != 0) {
    ExFreePool(DAT_0001ce04);
  }
  return;
}

