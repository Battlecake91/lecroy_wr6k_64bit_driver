
void FUN_00017f60(undefined4 *param_1,char param_2)

{
  undefined4 *puVar1;
  
  while (param_1 != (undefined4 *)0x0) {
    puVar1 = (undefined4 *)*param_1;
    if (param_2 != '\0') {
      MmUnlockPages(param_1);
    }
    IoFreeMdl(param_1);
    param_1 = puVar1;
  }
  return;
}

