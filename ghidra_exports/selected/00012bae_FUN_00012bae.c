
undefined4 __thiscall FUN_00012bae(void *this,int param_1)

{
  int iVar1;
  undefined4 *puVar2;
  undefined4 *puVar3;
  undefined4 local_24 [8];
  
  *(undefined4 *)(param_1 + 0x18) = 0xc0000002;
  if (*(int *)(*(int *)(param_1 + 0x60) + 4) == 0x1d) {
    puVar2 = local_24;
    for (iVar1 = 7; iVar1 != 0; iVar1 = iVar1 + -1) {
      *puVar2 = 0;
      puVar2 = puVar2 + 1;
    }
    *(undefined1 *)puVar2 = 0;
    local_24[2] = *(undefined4 *)((int)this + 0x116a);
    local_24[1] = 2;
    puVar2 = local_24;
    puVar3 = *(undefined4 **)(param_1 + 0xc);
    for (iVar1 = 7; iVar1 != 0; iVar1 = iVar1 + -1) {
      *puVar3 = *puVar2;
      puVar2 = puVar2 + 1;
      puVar3 = puVar3 + 1;
    }
    *(undefined1 *)puVar3 = *(undefined1 *)puVar2;
    *(undefined4 *)((int)this + 0x116a) = 0;
    *(undefined4 *)(param_1 + 0x18) = 0;
    *(undefined4 *)(param_1 + 0x1c) = 0x1d;
  }
  else {
    *(undefined4 *)(param_1 + 0x1c) = 0;
    *(undefined4 *)(param_1 + 0x18) = 0xc0000206;
  }
  return *(undefined4 *)(param_1 + 0x18);
}

