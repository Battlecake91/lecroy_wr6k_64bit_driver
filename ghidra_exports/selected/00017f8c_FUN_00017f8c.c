
undefined4 FUN_00017f8c(void)

{
  int iVar1;
  int unaff_EDI;
  
  iVar1 = ExAllocatePoolWithTag(0,0x33000,0x206d6457);
  *(int *)(unaff_EDI + 0x10) = iVar1;
  if (iVar1 != 0) {
    iVar1 = IoAllocateMdl(iVar1,0x33000,0,0,0);
    *(int *)(unaff_EDI + 0x14) = iVar1;
    if (iVar1 != 0) {
      MmBuildMdlForNonPagedPool(iVar1);
      return 0;
    }
  }
  return 0xc000009a;
}

