
int __fastcall FUN_0001c06a(int *param_1)

{
  int *piVar1;
  int *piVar2;
  int iVar3;
  
  piVar1 = (int *)*param_1;
  iVar3 = 0;
  if (piVar1 != param_1) {
    iVar3 = *piVar1;
    piVar2 = (int *)piVar1[1];
    *piVar2 = iVar3;
    *(int **)(iVar3 + 4) = piVar2;
    iVar3 = (int)piVar1 - param_1[2];
  }
  return iVar3;
}

