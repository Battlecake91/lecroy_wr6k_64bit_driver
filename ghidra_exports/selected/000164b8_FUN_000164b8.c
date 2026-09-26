
undefined2 __thiscall FUN_000164b8(void *this,char *param_1)

{
  uint uVar1;
  undefined2 uVar2;
  
  if ((param_1 == (char *)0x0) || (*(void **)((int)this + 0x162) == (void *)0x0)) {
    uVar2 = 8;
  }
  else {
    uVar1 = (uint)(param_1[1] != '\0');
    if (*param_1 != '\0') {
      uVar1 = uVar1 | 2;
    }
    FUN_000107fe(*(void **)((int)this + 0x162),uVar1);
    uVar2 = 0;
  }
  FUN_00015a88(this,uVar2);
  return uVar2;
}

