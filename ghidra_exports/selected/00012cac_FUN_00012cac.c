
undefined4 __thiscall FUN_00012cac(void *this,int param_1)

{
  int iVar1;
  undefined4 *puVar2;
  undefined4 *puVar3;
  undefined4 local_110 [67];
  
  *(undefined4 *)(param_1 + 0x18) = 0xc0000002;
  if (DAT_0001cd08 == 0) {
    if (*(int *)(*(int *)(param_1 + 0x60) + 8) == 0x10a) {
      puVar2 = *(undefined4 **)(param_1 + 0xc);
      puVar3 = local_110;
      for (iVar1 = 0x42; iVar1 != 0; iVar1 = iVar1 + -1) {
        *puVar3 = *puVar2;
        puVar2 = puVar2 + 1;
        puVar3 = puVar3 + 1;
      }
      *(undefined2 *)puVar3 = *(undefined2 *)puVar2;
      FUN_0001259a((void *)((int)this + 0x11ee),(int)local_110);
      *(undefined4 *)(param_1 + 0x18) = 0;
      *(undefined4 *)(param_1 + 0x1c) = 0;
      goto LAB_00012d1b;
    }
    *(undefined4 *)(param_1 + 0x18) = 0xc0000206;
  }
  else {
    *(undefined4 *)(param_1 + 0x18) = 0xc00000a3;
  }
  *(undefined4 *)(param_1 + 0x1c) = 0;
LAB_00012d1b:
  return *(undefined4 *)(param_1 + 0x18);
}

