
undefined4 __fastcall FUN_00016cb0(void *param_1)

{
  uint uVar1;
  
  if (*(int *)((int)param_1 + 0xc) == 0) {
    return 0xc00000a3;
  }
  FUN_00016c92(param_1,0);
  do {
    uVar1 = FUN_00016ca4((int)param_1);
  } while ((uVar1 & 1) != 0);
  uVar1 = FUN_00016ca4((int)param_1);
  if ((uVar1 & 2) == 0) {
    *(undefined1 *)((int)param_1 + 0x18) = 1;
    return 0;
  }
  *(undefined1 *)((int)param_1 + 0x18) = 0;
  return 0xc0000001;
}

