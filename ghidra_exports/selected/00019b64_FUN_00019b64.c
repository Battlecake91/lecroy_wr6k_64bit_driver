
undefined4 __fastcall FUN_00019b64(int *param_1)

{
  undefined4 uVar1;
  
  if ((*(byte *)(param_1 + 0x4e) & 2) == 0) {
                    /* WARNING: Could not recover jumptable at 0x00019b71. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    uVar1 = (**(code **)(*param_1 + 0x100))();
    return uVar1;
  }
  return 0;
}

