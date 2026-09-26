
undefined2 __thiscall FUN_0001634e(void *this,char *param_1)

{
  undefined2 uVar1;
  int iVar2;
  
  uVar1 = 8;
  if (((param_1 == (char *)0x0) || (*(undefined4 **)((int)this + 0x186) == (undefined4 *)0x0)) ||
     (*param_1 != '\x01')) {
    uVar1 = 4;
  }
  else {
    (**(code **)**(undefined4 **)((int)this + 0x186))(*(int *)(param_1 + 1) * -10000,0);
    iVar2 = KeWaitForSingleObject(*(undefined4 *)(*(int *)((int)this + 0x186) + 0xc),0,0,1,0);
    if (iVar2 == 0) {
      uVar1 = 0;
    }
    FUN_00015a88(this,uVar1);
  }
  return uVar1;
}

