
undefined4 __fastcall FUN_00019aec(int *param_1)

{
  undefined4 uVar1;
  
  if ((*(byte *)(param_1 + 0x40) & 0x20) == 0) {
                    /* WARNING: Could not recover jumptable at 0x00019af9. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    uVar1 = (**(code **)(*param_1 + 0xfc))();
    return uVar1;
  }
  return 0;
}

