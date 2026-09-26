
void __thiscall FUN_00011c36(void *this,int param_1)

{
  undefined4 uVar1;
  
  if (*(int *)(*(int *)(param_1 + 0x60) + 8) == 4) {
    uVar1 = (**(code **)(*(int *)this + 8))(*(undefined4 *)(param_1 + 0xc));
    *(undefined4 *)(param_1 + 0x18) = uVar1;
  }
  else {
    *(undefined4 *)(param_1 + 0x18) = 0xc000000d;
  }
  return;
}

