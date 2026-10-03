
int __thiscall FUN_000104f4(void *this,undefined4 param_1)

{
  undefined4 *puVar1;
  void *this_00;
  int *this_01;
  int iVar2;
  int local_20 [7];
  
  puVar1 = FUN_0001d868(local_20,L"CLecS65AcqDrvDevice",*(undefined4 *)((int)this + 0x18),10);
  this_00 = (void *)FUN_0001d756(0x1535,(short *)*puVar1,0x22,(short *)0x0,0,0x2004);
  if (this_00 == (void *)0x0) {
    this_01 = (int *)0x0;
  }
  else {
    this_01 = FUN_00010b3c(this_00,param_1,*(undefined4 *)((int)this + 0x18));
  }
  if (local_20[0] != 0) {
    FUN_0001d78a(local_20);
  }
  if (this_01 == (int *)0x0) {
    iVar2 = -0x3fffff66;
  }
  else {
    iVar2 = this_01[9];
    if (iVar2 < 0) {
      (**(code **)(*this_01 + 4))();
    }
    else {
      *(int *)((int)this + 0x18) = *(int *)((int)this + 0x18) + 1;
      FUN_000104a4(this_01,&param_1,1);
    }
  }
  return iVar2;
}

