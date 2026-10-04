
undefined4 __fastcall FUN_00019b4e(int *param_1)

{
  undefined4 uVar1;
  
  if ((*(byte *)(param_1 + 0x4e) & 1) == 0) {
                    /* WARNING: Could not recover jumptable at 0x00019b5b. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    uVar1 = (**(code **)(*param_1 + 0x100))();
    return uVar1;
  }
  return 0;
}

