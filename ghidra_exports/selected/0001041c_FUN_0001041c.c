
void FUN_0001041c(undefined4 param_1,undefined4 param_2)

{
  int iVar1;
  
  iVar1 = (**(code **)(*DAT_0001cdf8 + 0xc))(param_2);
  if (-1 < iVar1) {
    for (iVar1 = *(int *)(DAT_0001cdf8[1] + 4); iVar1 != 0; iVar1 = *(int *)(iVar1 + 0xc)) {
      *(byte *)(iVar1 + 0x1c) = *(byte *)(iVar1 + 0x1c) & 0x7f;
    }
  }
  return;
}

