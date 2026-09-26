
undefined4 __thiscall FUN_000122e2(void *this,int param_1)

{
  int *piVar1;
  int *piVar2;
  int *piVar3;
  
  piVar1 = *(int **)this;
  piVar3 = (int *)0x0;
  while( true ) {
    piVar2 = piVar1;
    if (piVar2 == (int *)0x0) {
      return 0xc0000001;
    }
    if (*piVar2 == param_1) break;
    piVar1 = (int *)piVar2[3];
    piVar3 = piVar2;
  }
  piVar2[1] = piVar2[1] + -1;
  if (piVar2[1] < 1) {
    if (piVar3 != (int *)0x0) {
      piVar3[3] = piVar2[3];
    }
    if (piVar2 == *(int **)this) {
      *(int *)this = piVar2[3];
    }
    ExFreePool(piVar2);
  }
  return 0;
}

