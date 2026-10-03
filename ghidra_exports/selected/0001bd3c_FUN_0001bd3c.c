
undefined4 __thiscall FUN_0001bd3c(void *this,ushort *param_1)

{
  ushort uVar1;
  undefined4 uVar2;
  
  uVar1 = *param_1;
  if (*(ushort *)((int)this + 2) < uVar1) {
    uVar2 = 0xc0000023;
  }
  else {
    *(ushort *)this = uVar1;
    memmove(*(void **)((int)this + 4),*(void **)(param_1 + 2),(uint)uVar1);
    uVar2 = 0;
  }
  return uVar2;
}

