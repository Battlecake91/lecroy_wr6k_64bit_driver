
void __thiscall FUN_0001875c(void *this,undefined4 param_1,undefined4 param_2,undefined4 param_3)

{
  int iVar1;
  int iVar2;
  undefined1 local_20 [4];
  int local_1c;
  undefined4 local_10;
  undefined4 local_c;
  undefined4 local_8;
  
  if (*(code **)((int)this + 0x20) == (code *)0x0) {
    FUN_0001b9c6(local_20,*(undefined4 *)this,0,&local_c);
    local_8 = CONCAT31(local_8._1_3_,*(undefined1 *)(local_1c + 0x30));
    iVar2 = IoAllocateIrp(local_8,0);
    iVar1 = *(int *)(iVar2 + 0x60);
    *(undefined4 *)(iVar1 + -0x20) = 0;
    *(undefined4 *)(iVar1 + -0x1c) = param_1;
    *(undefined4 *)(iVar1 + -0x18) = param_2;
    *(undefined4 *)(iVar1 + -0x14) = param_3;
    *(undefined1 *)(iVar1 + -0x24) = 0x1b;
    *(undefined1 *)(iVar1 + -0x23) = 0xf;
    *(undefined4 *)(iVar2 + 0x18) = 0xc00000bb;
    local_c = FUN_0001bed2(iVar2,1,&local_10);
    IoFreeIrp(iVar2);
    FUN_000105f2((int)local_20);
  }
  else {
    (**(code **)((int)this + 0x20))(*(undefined4 *)((int)this + 8),0,param_1,param_2,param_3);
  }
  return;
}

