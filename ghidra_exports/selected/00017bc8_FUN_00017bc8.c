
uint __thiscall FUN_00017bc8(void *this,int param_1,byte param_2,byte param_3)

{
  uint in_EAX;
  undefined4 uVar1;
  uint uVar2;
  int iVar3;
  
  if (*(char *)this == '\0') {
    uVar2 = in_EAX & 0xffffff00;
  }
  else {
    uVar2 = (uint)param_2;
    iVar3 = 0;
    if (uVar2 != 0) {
      do {
        FUN_000179e2(*(void **)((int)this + 4),*(uint *)(param_1 + iVar3 * 4));
        iVar3 = iVar3 + 1;
      } while (iVar3 < (int)uVar2);
    }
    uVar1 = FUN_000107fe(*(void **)((int)this + 8),(param_3 & 3) << 8 ^ uVar2);
    uVar2 = CONCAT31((int3)((uint)uVar1 >> 8),1);
  }
  return uVar2;
}

