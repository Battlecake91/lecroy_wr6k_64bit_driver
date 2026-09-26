
undefined4 __thiscall FUN_00010a8e(void *this,int param_1)

{
  int *piVar1;
  undefined4 *puVar2;
  int *piVar3;
  
  piVar1 = *(int **)this;
  if (*(int **)this == (int *)0x0) {
    piVar1 = (int *)ExAllocatePoolWithTag(0,0x10,0x4c43654c);
    if (piVar1 == (int *)0x0) {
      return 0xc0000017;
    }
    *piVar1 = 0;
    piVar1[1] = 0;
    piVar1[2] = 0;
    piVar1[3] = 0;
    *(int **)this = piVar1;
  }
  do {
    piVar3 = piVar1;
    if (*piVar3 == 0) goto LAB_00010afe;
    if (*piVar3 == param_1) {
      piVar3[1] = piVar3[1] + 1;
      return 0;
    }
    piVar1 = (int *)piVar3[3];
  } while ((int *)piVar3[3] != (int *)0x0);
  puVar2 = (undefined4 *)ExAllocatePoolWithTag(0,0x10,0x4c43654c);
  piVar3[3] = (int)puVar2;
  if (puVar2 == (undefined4 *)0x0) {
    return 0xc0000017;
  }
  *puVar2 = 0;
  puVar2[1] = 0;
  puVar2[2] = 0;
  puVar2[3] = 0;
  piVar3 = (int *)piVar3[3];
LAB_00010afe:
  piVar3[1] = 1;
  *piVar3 = param_1;
  return 0;
}

