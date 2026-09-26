
undefined2 __thiscall FUN_000163b2(void *this,char *param_1)

{
  undefined2 uVar1;
  undefined4 uVar2;
  
  uVar1 = 0;
  if (param_1 == (char *)0x0) {
    uVar1 = 8;
  }
  else {
    if (*(int *)((int)this + 0x186) != 0) {
      KeWaitForSingleObject(*(undefined4 *)(*(int *)((int)this + 0x186) + 0xc),0,0,1,0);
    }
    if (*param_1 == '\0') {
      uVar2 = 0;
    }
    else {
      if (*param_1 != '\x01') {
        uVar1 = 4;
        goto LAB_00016403;
      }
      uVar2 = 1;
    }
    WRITE_REGISTER_ULONG(*(int *)(*(int *)((int)this + 0x29) + 0x10) + 0x80,uVar2);
  }
LAB_00016403:
  FUN_00015a88(this,uVar1);
  return uVar1;
}

