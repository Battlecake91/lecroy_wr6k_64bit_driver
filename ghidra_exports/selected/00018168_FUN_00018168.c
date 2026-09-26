
int __thiscall FUN_00018168(void *this,int param_1,int *param_2)

{
  int iVar1;
  int iVar2;
  
  iVar2 = *(int *)((int)this + 4);
  if (param_2 != (int *)0x0) {
    *param_2 = 0;
  }
  for (; (iVar1 = 0, iVar2 != 0 && (iVar1 = iVar2, iVar2 != param_1));
      iVar2 = *(int *)(iVar2 + 0x3c)) {
    if (param_2 != (int *)0x0) {
      *param_2 = iVar2;
    }
  }
  return iVar1;
}

