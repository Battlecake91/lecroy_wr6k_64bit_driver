
void __thiscall FUN_0001bffa(void *this,int param_1)

{
  int *piVar1;
  int *piVar2;
  
  piVar2 = *(int **)((int)this + 4);
  piVar1 = (int *)((int)this + 8);
  *(void **)(param_1 + *piVar1) = this;
  *(int **)(*piVar1 + 4 + param_1) = piVar2;
  *piVar2 = *piVar1 + param_1;
  *(int *)((int)this + 4) = *piVar1 + param_1;
  return;
}

