
int __thiscall FUN_000130ea(void *this,int param_1)

{
  undefined4 *puVar1;
  int iVar2;
  uint *puVar3;
  void *local_c;
  undefined1 local_8;
  undefined2 uStack_7;
  undefined1 uStack_5;
  
  local_8 = SUB41(this,0);
  uStack_7 = (undefined2)((uint)this >> 8);
  uStack_5 = (undefined1)((uint)this >> 0x18);
  if (*(int *)(param_1 + 0xc) == 0) {
    iVar2 = -0x3ffffff3;
    *(undefined4 *)(param_1 + 0x18) = 0xc000000d;
  }
  else if (*(int *)(*(int *)(param_1 + 0x60) + 4) == 8) {
    local_c = (void *)0x0;
    local_8 = 0;
    uStack_7 = 0;
    uStack_5 = 0;
    iVar2 = FUN_00016fac((void *)((int)this + 0x12ab),&local_c);
    puVar1 = *(undefined4 **)(param_1 + 0xc);
    *puVar1 = local_c;
    puVar1[1] = CONCAT13(uStack_5,CONCAT21(uStack_7,local_8));
    *(uint *)(param_1 + 0x1c) = (-(uint)(iVar2 != 0) & 0xfffffff8) + 8;
    *(int *)(param_1 + 0x18) = iVar2;
  }
  else {
    *(undefined4 *)((int)this + 0x1272) = 3;
    local_c = this;
    puVar3 = FUN_00010750((uint *)((int)this + 0x1256));
    FUN_00010750(puVar3);
    iVar2 = -0x3ffffdfa;
  }
  return iVar2;
}

