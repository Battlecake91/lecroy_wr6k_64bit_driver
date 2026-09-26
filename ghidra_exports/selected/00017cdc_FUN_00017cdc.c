
uint __thiscall FUN_00017cdc(void *this,int param_1)

{
  uint in_EAX;
  int iVar1;
  
  if ((param_1 != 0) && (in_EAX = FUN_00017b72(param_1), (char)in_EAX != '\0')) {
    iVar1 = 0;
    if (*(char *)(param_1 + 0x78) != '\0') {
      do {
        FUN_000179e2(*(void **)((int)this + 0xc),*(uint *)(*(int *)(param_1 + 0x6c) + iVar1 * 4));
        in_EAX = (uint)*(byte *)(param_1 + 0x78);
        iVar1 = iVar1 + 1;
      } while (iVar1 < (int)in_EAX);
    }
    return CONCAT31((int3)(in_EAX >> 8),1);
  }
  return in_EAX & 0xffffff00;
}

