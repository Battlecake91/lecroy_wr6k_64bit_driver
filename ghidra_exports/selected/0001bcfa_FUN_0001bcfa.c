
undefined2 * __thiscall FUN_0001bcfa(void *this,ushort param_1,undefined4 param_2)

{
  int iVar1;
  
  *(undefined2 *)this = 0;
  iVar1 = FUN_0001039a((uint)(param_1 >> 1) << 1,param_2);
  *(int *)((int)this + 4) = iVar1;
  if (iVar1 == 0) {
    *(undefined1 *)((int)this + 8) = 0;
    *(undefined2 *)((int)this + 2) = 0;
  }
  else {
    *(undefined1 *)((int)this + 8) = 1;
    *(ushort *)((int)this + 2) = param_1;
  }
  return this;
}

