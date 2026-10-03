
void __thiscall FUN_00010656(void *this,undefined4 *param_1)

{
  int iVar1;
  undefined4 uVar2;
  
  iVar1 = *(int *)this;
  if (iVar1 == 0) {
    uVar2 = 0xffffffff;
    *param_1 = 0xffffffff;
  }
  else {
    *param_1 = *(undefined4 *)(iVar1 + 4);
    uVar2 = *(undefined4 *)(iVar1 + 8);
  }
  param_1[1] = uVar2;
  return;
}

