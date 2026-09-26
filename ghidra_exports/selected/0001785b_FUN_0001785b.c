
void __thiscall FUN_0001785b(void *this,int param_1,void *param_2)

{
  *(int *)((int)this + 0x10) = *(int *)(param_1 + 0x10) + 0x400;
  *(undefined4 *)((int)this + 0x2c) = 0x400;
  *(char **)((int)this + 0x1c) = "TxControl";
  *(undefined **)((int)this + 0x20) = &DAT_00017802;
  *(undefined4 *)((int)this + 0x28) = 9;
  *(undefined4 *)((int)this + 0x24) = 4;
  *(int *)((int)this + 0x38) = *(int *)(param_1 + 0x10) + 0x404;
  *(undefined4 *)((int)this + 0x4c) = 4;
  *(char **)((int)this + 0x44) = "RxControl";
  *(undefined **)((int)this + 0x48) = &DAT_00017816;
  *(undefined4 *)((int)this + 0x50) = 9;
  *(undefined4 *)((int)this + 0x54) = 0x404;
  *(int *)((int)this + 0x60) = *(int *)(param_1 + 0x10) + 0x408;
  *(undefined4 *)((int)this + 0x74) = 4;
  *(char **)((int)this + 0x6c) = "TxCount";
  *(undefined **)((int)this + 0x70) = &DAT_00017826;
  *(undefined4 *)((int)this + 0x78) = 7;
  *(undefined4 *)((int)this + 0x7c) = 0x408;
  *(int *)((int)this + 0x88) = *(int *)(param_1 + 0x10) + 0x40c;
  *(undefined4 *)((int)this + 0x9c) = 4;
  *(char **)((int)this + 0x94) = "RxCount";
  *(undefined **)((int)this + 0x98) = &DAT_00017836;
  *(undefined4 *)((int)this + 0xa0) = 7;
  *(undefined4 *)((int)this + 0xa4) = 0x40c;
  *(int *)((int)this + 0xb0) = *(int *)(param_1 + 0x10) + 0x100;
  *(undefined4 *)((int)this + 0xc4) = 4;
  *(char **)((int)this + 0xbc) = "SetIRQ";
  *(undefined **)((int)this + 0xc0) = &DAT_00017846;
  *(undefined4 *)((int)this + 200) = 6;
  *(undefined4 *)((int)this + 0xcc) = 0x100;
  *(int *)((int)this + 0xd8) = *(int *)(param_1 + 0x10) + 0x410;
  *(undefined4 *)((int)this + 0xec) = 4;
  *(char **)((int)this + 0xe4) = "HWInt";
  *(undefined **)((int)this + 0xe8) = &DAT_00017856;
  *(undefined4 *)((int)this + 0xf0) = 5;
  *(undefined4 *)((int)this + 0xf4) = 0x410;
  FUN_00013fa6(param_2,(int)this + 0x10);
  FUN_00013fa6(param_2,(int)this + 0x38);
  FUN_00013fa6(param_2,(int)this + 0x60);
  FUN_00013fa6(param_2,(int)this + 0x88);
  FUN_00013fa6(param_2,(int)this + 0xb0);
  FUN_00013fa6(param_2,(int)this + 0xd8);
  *(int *)((int)this + 0x100) = param_1;
  KeInitializeEvent(this,0,0);
  return;
}

