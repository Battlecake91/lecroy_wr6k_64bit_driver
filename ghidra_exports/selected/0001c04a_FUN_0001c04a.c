
void __thiscall FUN_0001c04a(void *this,int param_1)

{
  int iVar1;
  int *piVar2;
  
  if (param_1 != 0) {
    piVar2 = (int *)(*(int *)((int)this + 8) + param_1);
    iVar1 = *piVar2;
    piVar2 = (int *)piVar2[1];
    *piVar2 = iVar1;
    *(int **)(iVar1 + 4) = piVar2;
  }
  return;
}

