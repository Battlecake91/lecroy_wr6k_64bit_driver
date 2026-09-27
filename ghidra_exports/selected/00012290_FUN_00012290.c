
undefined1 __thiscall FUN_00012290(void *this,int param_1,undefined4 param_2)

{
  int iVar1;
  undefined1 uVar2;
  
  uVar2 = 0;
  if (param_1 <= *(int *)((int)this + 0xc)) {
    iVar1 = *(int *)(*(int *)((int)this + 0x10) + param_1 * 4);
    if (iVar1 != 0) {
      *(undefined4 *)(iVar1 + 4) = param_2;
    }
    if (*(int *)((int)this + 0x48) != 0) {
      *(undefined4 *)((param_1 + 1) * 0x108 + *(int *)((int)this + 0x48)) = param_2;
    }
    uVar2 = 1;
    FUN_00018a1c((uint *)((int)this + 0x1c),1,"Trace %s Set to Level %d\n");
  }
  return uVar2;
}

