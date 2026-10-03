
int __fastcall FUN_0001944c(int param_1)

{
  int iVar1;
  LONG unaff_ESI;
  LONG *unaff_EDI;
  undefined4 *puVar2;
  
  iVar1 = *(int *)(param_1 + 0x24);
  if (-1 < iVar1) {
    if ((((*(int *)(param_1 + 0x44) != 0) && (*(int *)(param_1 + 0x70) != 0)) &&
        (*(int *)(param_1 + 0x90) != 0)) && (*(int *)(param_1 + 0x1b4) != 0)) {
      *(undefined1 *)(param_1 + 0x28) = 1;
      InterlockedExchange(unaff_EDI,unaff_ESI);
      puVar2 = (undefined4 *)(param_1 + 200);
      for (iVar1 = 0x10; iVar1 != 0; iVar1 = iVar1 + -1) {
        *puVar2 = 0;
        puVar2 = puVar2 + 1;
      }
      puVar2 = (undefined4 *)(param_1 + 0x110);
      for (iVar1 = 0x12; iVar1 != 0; iVar1 = iVar1 + -1) {
        *puVar2 = 0;
        puVar2 = puVar2 + 1;
      }
      puVar2 = (undefined4 *)(param_1 + 0x164);
      for (iVar1 = 0x10; iVar1 != 0; iVar1 = iVar1 + -1) {
        *puVar2 = 0;
        puVar2 = puVar2 + 1;
      }
      *(undefined4 *)(param_1 + 0x158) = 0;
      *(undefined4 *)(param_1 + 0x15c) = 0;
      *(undefined4 *)(param_1 + 0x160) = 0;
      return 0;
    }
    iVar1 = -0x3fffff66;
  }
  return iVar1;
}

