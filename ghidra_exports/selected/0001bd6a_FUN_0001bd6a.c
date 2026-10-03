
undefined4 __thiscall FUN_0001bd6a(void *this,short *param_1)

{
  uint uVar1;
  undefined4 uVar2;
  
  uVar1 = FUN_0001bcce(param_1);
  if (*(ushort *)((int)this + 2) < (ushort)uVar1) {
    uVar2 = 0xc0000023;
  }
  else {
    *(ushort *)this = (ushort)uVar1;
    memmove(*(void **)((int)this + 4),param_1,uVar1 & 0xffff);
    uVar2 = 0;
  }
  return uVar2;
}

