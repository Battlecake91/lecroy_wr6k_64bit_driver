
undefined2 __thiscall FUN_0001600e(void *this,char *param_1)

{
  char cVar1;
  int iVar2;
  undefined2 uVar3;
  
  cVar1 = *param_1;
  uVar3 = 0;
  if (cVar1 == '\0') {
    iVar2 = *(int *)((int)this + 0x25);
  }
  else if (cVar1 == '\x01') {
    iVar2 = *(int *)((int)this + 0x29);
  }
  else {
    if (cVar1 != '\x02') {
      uVar3 = 4;
      goto LAB_00016054;
    }
    iVar2 = *(int *)((int)this + 0x2d);
  }
  WRITE_REGISTER_ULONG
            ((uint)*(ushort *)(param_1 + 1) + *(int *)(iVar2 + 0x10),*(undefined4 *)(param_1 + 3));
LAB_00016054:
  FUN_00015a88(this,uVar3);
  return uVar3;
}

