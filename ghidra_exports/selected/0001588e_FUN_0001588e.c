
char __thiscall FUN_0001588e(void *this,uint param_1)

{
  uint uVar1;
  int iVar2;
  
  uVar1 = 0;
  if (*(char *)this != '\0') {
    iVar2 = 0;
    do {
      if ((param_1 & 0x80000000) != 0) {
        uVar1 = uVar1 | 1 << ((byte)iVar2 & 0x1f);
      }
      param_1 = param_1 << 1;
      iVar2 = iVar2 + 1;
    } while (iVar2 < 0x20);
    FUN_000107fe(*(void **)((int)this + 8),uVar1);
  }
  return *(char *)this;
}

