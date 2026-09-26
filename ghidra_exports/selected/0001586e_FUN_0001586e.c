
char __fastcall FUN_0001586e(char *param_1)

{
  undefined4 uVar1;
  undefined4 *unaff_retaddr;
  
  if (*param_1 != '\0') {
    uVar1 = READ_REGISTER_ULONG(**(undefined4 **)(param_1 + 0xc));
    *unaff_retaddr = uVar1;
  }
  return *param_1;
}

