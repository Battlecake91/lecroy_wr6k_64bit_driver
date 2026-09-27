
int __thiscall FUN_000120fa(void *this,int param_1,int param_2,int param_3)

{
  undefined4 *puVar1;
  uint uVar2;
  uint uVar3;
  
  uVar3 = param_1 * 0x10a;
  *(undefined4 *)this = 0;
  *(undefined4 *)((int)this + 0xc) = 0xffffffff;
  *(undefined4 *)((int)this + 0x14) = 0;
  *(int *)((int)this + 4) = param_2;
  *(int *)((int)this + 8) = param_3;
  puVar1 = (undefined4 *)FUN_0001039a(uVar3,param_3);
  *(undefined4 **)((int)this + 0x10) = puVar1;
  if ((puVar1 == (undefined4 *)0x0) && (param_1 != 0)) {
    *(undefined4 *)((int)this + 4) = 0;
    *(undefined4 *)((int)this + 0x14) = 0xc000009a;
  }
  else {
    *(int *)this = param_1;
    for (uVar2 = uVar3 >> 2; uVar2 != 0; uVar2 = uVar2 - 1) {
      *puVar1 = 0;
      puVar1 = puVar1 + 1;
    }
    for (uVar3 = uVar3 & 3; uVar3 != 0; uVar3 = uVar3 - 1) {
      *(undefined1 *)puVar1 = 0;
      puVar1 = (undefined4 *)((int)puVar1 + 1);
    }
  }
  return *(int *)((int)this + 0x14);
}

