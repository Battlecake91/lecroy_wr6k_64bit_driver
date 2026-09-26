
uint __thiscall FUN_00017a98(void *this,uint param_1,uint param_2,uint param_3)

{
  undefined4 *puVar1;
  uint uVar2;
  int iVar3;
  
  *(undefined4 *)this = 0;
  *(undefined4 *)((int)this + 0xc) = 0xffffffff;
  *(undefined4 *)((int)this + 0x14) = 0;
  *(uint *)((int)this + 4) = param_2;
  *(uint *)((int)this + 8) = param_3;
  puVar1 = (undefined4 *)FUN_0001039a(param_1 << 2,param_3);
  *(undefined4 **)((int)this + 0x10) = puVar1;
  if ((puVar1 == (undefined4 *)0x0) && (param_1 != 0)) {
    *(undefined4 *)((int)this + 4) = 0;
    *(undefined4 *)((int)this + 0x14) = 0xc000009a;
  }
  else {
    uVar2 = param_1 & 0x3fffffff;
    *(uint *)this = param_1;
    for (; uVar2 != 0; uVar2 = uVar2 - 1) {
      *puVar1 = 0;
      puVar1 = puVar1 + 1;
    }
    for (iVar3 = 0; iVar3 != 0; iVar3 = iVar3 + -1) {
      *(undefined1 *)puVar1 = 0;
      puVar1 = (undefined4 *)((int)puVar1 + 1);
    }
  }
  return *(uint *)((int)this + 0x14);
}

