
int __thiscall FUN_0001c024(void *this,int param_1)

{
  void *pvVar1;
  
  if (param_1 == 0) {
    if (*(void **)this != this) {
      return (int)*(void **)this - *(int *)((int)this + 8);
    }
  }
  else {
    pvVar1 = *(void **)(*(int *)((int)this + 8) + param_1);
    if (pvVar1 != this) {
      return (int)pvVar1 - *(int *)((int)this + 8);
    }
  }
  return 0;
}

