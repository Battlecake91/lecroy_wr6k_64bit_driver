
/* WARNING: Function: __SEH_epilog replaced with injection: EH_epilog3 */

void FUN_00017fd6(void)

{
  int *piVar1;
  int iVar2;
  undefined4 *puVar3;
  undefined4 extraout_ECX;
  int unaff_EBP;
  
  FUN_0001c0a8(&DAT_0001c9d0,0x2c);
  *(undefined4 *)(unaff_EBP + -0x1c) = extraout_ECX;
  piVar1 = *(int **)(unaff_EBP + 8);
  if ((piVar1 != (int *)0x0) && (iVar2 = *piVar1, iVar2 != 0)) {
    *(undefined4 *)(unaff_EBP + -4) = 0;
    iVar2 = *(int *)(iVar2 + 0x14);
    if (iVar2 != 0) {
      *(int *)(unaff_EBP + -0x20) = iVar2;
      *(undefined4 *)(unaff_EBP + -0x24) = 1;
      IoFreeMdl(iVar2);
      *(undefined4 *)(*piVar1 + 0x14) = 0;
    }
    iVar2 = *(int *)(*piVar1 + 0x10);
    if (iVar2 != 0) {
      *(int *)(unaff_EBP + -0x28) = iVar2;
      *(undefined4 *)(unaff_EBP + -0x2c) = 1;
      ExFreePool(iVar2);
      *(undefined4 *)(*piVar1 + 0x10) = 0;
    }
    iVar2 = *piVar1;
    puVar3 = *(undefined4 **)(iVar2 + 8);
    if (puVar3 != (undefined4 *)0x0) {
      *(undefined4 **)(unaff_EBP + -0x30) = puVar3;
      *(undefined4 *)(unaff_EBP + -0x34) = 1;
      FUN_00017f60(puVar3,*(char *)(iVar2 + 0x38));
      *(undefined4 *)(*piVar1 + 8) = 0;
    }
    iVar2 = *piVar1;
    *(int *)(unaff_EBP + -0x38) = iVar2;
    *(undefined4 *)(unaff_EBP + -0x3c) = 1;
    ExFreePool(iVar2);
    *piVar1 = 0;
    *(undefined4 *)(unaff_EBP + -4) = 0xffffffff;
  }
  return;
}

