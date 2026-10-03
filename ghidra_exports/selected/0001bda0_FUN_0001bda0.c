
undefined4 __thiscall FUN_0001bda0(void *this,short param_1,undefined4 param_2)

{
  undefined4 uVar1;
  undefined4 local_10;
  int local_c;
  uint local_8;
  
  FUN_0001bcfa(&local_10,param_1 * 2 + *(short *)((int)this + 2),param_2);
  if (local_c == 0) {
    uVar1 = 0xc000009a;
  }
  else {
    FUN_0001bd3c(&local_10,this);
    if ((*(char *)((int)this + 8) != '\0') && (*(int *)((int)this + 4) != 0)) {
      ExFreePool(*(int *)((int)this + 4));
    }
    *(undefined4 *)this = local_10;
    *(int *)((int)this + 4) = local_c;
    *(uint *)((int)this + 8) = local_8;
    local_8 = local_8 & 0xffffff00;
    *(undefined1 *)((int)this + 8) = 1;
    if (local_c != 0) {
      FUN_000105cc((undefined2 *)&local_10);
    }
    uVar1 = 0;
  }
  return uVar1;
}

