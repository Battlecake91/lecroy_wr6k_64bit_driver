
/* WARNING: Function: __SEH_epilog replaced with injection: EH_epilog3 */

undefined4 FUN_0001807a(void)

{
  int *piVar1;
  int iVar2;
  int *piVar3;
  undefined4 uVar4;
  int iVar5;
  uint3 uVar7;
  int *piVar6;
  int unaff_EBP;
  int iVar8;
  
  FUN_0001c0a8(&DAT_0001c9e0,0xc);
  *(undefined4 *)(unaff_EBP + -0x1c) = 0;
  if (*(uint *)(unaff_EBP + 0xc) < 0x6000001) {
    *(undefined4 *)(unaff_EBP + -4) = 0;
    if ((*(char *)(unaff_EBP + 0x18) != '\0') && (*(int *)(unaff_EBP + 0x1c) == 1)) {
      ProbeForWrite(*(undefined4 *)(unaff_EBP + 8),*(undefined4 *)(unaff_EBP + 0xc),4);
    }
    *(undefined4 *)(unaff_EBP + -4) = 0xffffffff;
    piVar1 = *(int **)(unaff_EBP + 0x14);
    iVar2 = *(int *)(unaff_EBP + 0x10);
    while (*(int *)(unaff_EBP + 0xc) != 0) {
      iVar8 = 0x2000000;
      if (*(uint *)(unaff_EBP + 0xc) < 0x2000001) {
        iVar8 = *(int *)(unaff_EBP + 0xc);
      }
      uVar7 = (uint3)((uint)*(int *)(unaff_EBP + 8) >> 8);
      if ((iVar2 == 0) || (*(int *)(iVar2 + 4) == 0)) {
        iVar5 = (uint)uVar7 << 8;
      }
      else {
        iVar5 = CONCAT31(uVar7,1);
      }
      iVar5 = IoAllocateMdl(*(int *)(unaff_EBP + -0x1c) + *(int *)(unaff_EBP + 8),iVar8,iVar5,1,
                            iVar2);
      if (iVar5 == 0) {
        return 0xc000009a;
      }
      if (iVar2 == 0) {
        piVar6 = (int *)*piVar1;
        if (piVar6 == (int *)0x0) {
          *piVar1 = iVar5;
        }
        else {
          do {
            piVar3 = (int *)*piVar6;
            if (piVar3 == (int *)0x0) break;
            piVar6 = piVar3;
          } while (piVar3 != (int *)0x0);
          *piVar6 = iVar5;
        }
      }
      *(undefined4 *)(unaff_EBP + -4) = 1;
      if (*(char *)(unaff_EBP + 0x18) != '\0') {
        MmProbeAndLockPages(iVar5,*(undefined4 *)(unaff_EBP + 0x1c),1);
      }
      *(undefined4 *)(unaff_EBP + -4) = 0xffffffff;
      *(int *)(unaff_EBP + -0x1c) = *(int *)(unaff_EBP + -0x1c) + iVar8;
      *(int *)(unaff_EBP + 0xc) = *(int *)(unaff_EBP + 0xc) - iVar8;
    }
    uVar4 = 0;
  }
  else {
    uVar4 = 0xc000000d;
  }
  return uVar4;
}

