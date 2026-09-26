
undefined4 __thiscall FUN_0001286e(void *this,int param_1)

{
  uint uVar1;
  char cVar2;
  undefined4 uVar3;
  
  cVar2 = '\0';
  uVar1 = *(uint *)(*(int *)(param_1 + 0x60) + 8);
  if (uVar1 < 4) {
    uVar3 = 0xc0000206;
    *(undefined4 *)(param_1 + 0x18) = 0xc0000206;
  }
  else {
    if (uVar1 == 5) {
      cVar2 = (char)(*(int **)(param_1 + 0xc))[1];
    }
    FUN_0001557c((void *)((int)this + 0x10f2),**(int **)(param_1 + 0xc),cVar2);
    *(undefined4 *)(param_1 + 0x18) = 0;
    uVar3 = 0;
  }
  return uVar3;
}

