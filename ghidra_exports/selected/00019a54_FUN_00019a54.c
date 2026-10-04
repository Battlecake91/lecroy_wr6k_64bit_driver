
undefined4 __fastcall FUN_00019a54(int *param_1)

{
  undefined4 uVar1;
  
  if (((*(byte *)(param_1 + 0x40) & 4) == 0) && ((*(byte *)(param_1 + 0x3f) & 0x20) == 0)) {
                    /* WARNING: Could not recover jumptable at 0x00019a6a. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    uVar1 = (**(code **)(*param_1 + 0xfc))();
    return uVar1;
  }
  return 0;
}

