
undefined4 FUN_00012832(int param_1)

{
  undefined4 uVar1;
  
  if (*(undefined4 **)(param_1 + 0xc) == (undefined4 *)0x0) {
    uVar1 = 0xc000000d;
    *(undefined4 *)(param_1 + 0x18) = 0xc000000d;
  }
  else if (*(int *)(*(int *)(param_1 + 0x60) + 4) == 4) {
    **(undefined4 **)(param_1 + 0xc) = 0x3ea;
    *(undefined4 *)(param_1 + 0x18) = 0;
    *(undefined4 *)(param_1 + 0x1c) = 4;
    uVar1 = 0;
  }
  else {
    uVar1 = 0xc0000206;
  }
  return uVar1;
}

