
void __thiscall FUN_00012ada(void *this,int param_1)

{
  undefined4 uVar1;
  int iVar2;
  undefined4 *puVar3;
  undefined4 *puVar4;
  undefined4 local_10c [64];
  undefined4 local_c;
  int local_8;
  
  *(undefined4 *)(param_1 + 0x18) = 0xc0000002;
  if (*(int *)(*(int *)(param_1 + 0x60) + 8) == 0x108) {
    puVar3 = *(undefined4 **)(param_1 + 0xc);
    puVar4 = local_10c;
    for (iVar2 = 0x42; iVar2 != 0; iVar2 = iVar2 + -1) {
      *puVar4 = *puVar3;
      puVar3 = puVar3 + 1;
      puVar4 = puVar4 + 1;
    }
    FUN_00012290((void *)((int)this + 0x11a2),local_8,local_c);
    uVar1 = 0;
  }
  else {
    uVar1 = 0xc0000206;
  }
  *(undefined4 *)(param_1 + 0x1c) = 0;
  *(undefined4 *)(param_1 + 0x18) = uVar1;
  return;
}

