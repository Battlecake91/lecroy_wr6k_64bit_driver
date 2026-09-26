
void __thiscall FUN_000119bc(void *this,undefined4 *param_1)

{
  undefined4 uVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  
  *(undefined4 *)this = *param_1;
  *(undefined4 *)((int)this + 0x24) = param_1[9];
  *(undefined1 *)((int)this + 0x20) = *(undefined1 *)(param_1 + 8);
  *(undefined4 *)((int)this + 0x1c) = param_1[7];
  uVar1 = param_1[4];
  uVar2 = param_1[7];
  uVar3 = param_1[6];
  uVar4 = param_1[3];
  *(undefined4 *)((int)this + 0x14) = param_1[5];
  *(undefined4 *)((int)this + 0x18) = uVar3;
  *(undefined4 *)((int)this + 0x10) = uVar1;
  *(undefined4 *)((int)this + 0xc) = uVar4;
  *(undefined4 *)((int)this + 0x1c) = uVar2;
  return;
}

