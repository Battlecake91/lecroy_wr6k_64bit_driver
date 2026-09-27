
undefined4 __thiscall FUN_00012d24(void *this,int param_1)

{
  undefined4 uVar1;
  
  *(undefined4 *)(param_1 + 0x18) = 0xc0000002;
  if (*(int *)(*(int *)(param_1 + 0x60) + 4) == 4) {
    uVar1 = READ_REGISTER_ULONG(*(undefined4 *)((int)this + 0x138));
    **(undefined4 **)(param_1 + 0xc) = uVar1;
    *(undefined4 *)(param_1 + 0x18) = 0;
    *(undefined4 *)(param_1 + 0x1c) = 4;
    uVar1 = 0;
  }
  else {
    *(undefined4 *)(param_1 + 0x1c) = 0;
    uVar1 = 0xc0000206;
    *(undefined4 *)(param_1 + 0x18) = 0xc0000206;
  }
  return uVar1;
}

