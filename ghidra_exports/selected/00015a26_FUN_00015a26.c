
void __thiscall FUN_00015a26(void *this,undefined4 *param_1,ushort param_2)

{
  undefined4 *puVar1;
  uint uVar2;
  
  *(undefined1 *)((int)this + 0x24) = 0;
  if ((*(short *)((int)this + 0x21) != 0) && (*(int *)((int)this + 0x1d) != 0)) {
    ExFreePool(*(int *)((int)this + 0x1d));
    *(undefined4 *)((int)this + 0x1d) = 0;
    *(undefined2 *)((int)this + 0x21) = 0;
  }
  puVar1 = (undefined4 *)FUN_00010380((uint)param_2);
  *(undefined4 **)((int)this + 0x1d) = puVar1;
  if (puVar1 != (undefined4 *)0x0) {
    for (uVar2 = (uint)(param_2 >> 2); uVar2 != 0; uVar2 = uVar2 - 1) {
      *puVar1 = *param_1;
      param_1 = param_1 + 1;
      puVar1 = puVar1 + 1;
    }
    for (uVar2 = param_2 & 3; uVar2 != 0; uVar2 = uVar2 - 1) {
      *(undefined1 *)puVar1 = *(undefined1 *)param_1;
      param_1 = (undefined4 *)((int)param_1 + 1);
      puVar1 = (undefined4 *)((int)puVar1 + 1);
    }
    *(undefined1 *)((int)this + 0x23) = 1;
    *(ushort *)((int)this + 0x21) = param_2;
  }
  return;
}

