
undefined4 __thiscall FUN_00011bdc(void *this,int param_1)

{
  undefined4 uVar1;
  int iVar2;
  
  if (*(int *)(*(int *)(param_1 + 0x60) + 8) == 0xc) {
    iVar2 = (**(code **)(*(int *)this + 0xc))(*(undefined4 *)(param_1 + 0xc),(int)this + 0xe98,1,1);
    if (iVar2 == 0) {
      *(undefined4 *)(param_1 + 0xc) = 0;
      *(undefined4 *)(param_1 + 0x1c) = 0;
      *(undefined4 *)(param_1 + 0x18) = 0xc0000001;
    }
    else {
      **(int **)(param_1 + 0xc) = iVar2;
      *(undefined4 *)(param_1 + 0x1c) = 4;
      *(undefined4 *)(param_1 + 0x18) = 0;
    }
    uVar1 = *(undefined4 *)(param_1 + 0x18);
  }
  else {
    uVar1 = 0xc000000d;
    *(undefined4 *)(param_1 + 0x18) = 0xc000000d;
  }
  return uVar1;
}

