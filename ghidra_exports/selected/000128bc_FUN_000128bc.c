
undefined4 __thiscall FUN_000128bc(void *this,int param_1)

{
  undefined4 uVar1;
  
  if (*(int *)(*(int *)(param_1 + 0x60) + 8) == 1) {
    FUN_0001552c((void *)((int)this + 0x10f2),**(undefined1 **)(param_1 + 0xc));
    *(undefined4 *)(param_1 + 0x18) = 0;
    uVar1 = 0;
  }
  else {
    uVar1 = 0xc0000206;
    *(undefined4 *)(param_1 + 0x18) = 0xc0000206;
  }
  return uVar1;
}

