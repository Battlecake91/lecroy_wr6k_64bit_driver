
undefined4 __fastcall FUN_00013768(undefined4 *param_1)

{
  int iVar1;
  int *piVar2;
  
  iVar1 = IoGetCurrentProcess();
  piVar2 = (int *)param_1[0x3a6];
  while( true ) {
    if (piVar2 == (int *)0x0) {
      return 0;
    }
    if (*piVar2 == iVar1) break;
    piVar2 = (int *)piVar2[3];
  }
  if (piVar2[1] == 1) {
    if ((*(byte *)(piVar2 + 2) & 1) != 0) {
      (**(code **)*param_1)(iVar1);
    }
    if ((piVar2[1] == 1) && ((*(byte *)(piVar2 + 2) & 2) != 0)) {
      FUN_00012e72(param_1,iVar1);
    }
  }
  FUN_000122e2(param_1 + 0x3a6,iVar1);
  return 0;
}

