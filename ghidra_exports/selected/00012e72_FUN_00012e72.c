
undefined4 __thiscall FUN_00012e72(void *this,int param_1)

{
  int iVar1;
  undefined4 *puVar2;
  int iVar3;
  
  iVar1 = *(int *)((int)this + 0x115a);
  iVar3 = 0;
  if (-1 < iVar1) {
    do {
      puVar2 = *(undefined4 **)(*(int *)((int)this + 0x115e) + iVar3 * 4);
      if ((puVar2 != (undefined4 *)0x0) && (puVar2[3] == param_1)) {
        FUN_00011b48(puVar2);
      }
      iVar3 = iVar3 + 1;
    } while (iVar3 <= iVar1);
  }
  return 0xc0000002;
}

