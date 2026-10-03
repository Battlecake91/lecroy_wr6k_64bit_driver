
undefined4 * __thiscall FUN_0001a0b6(void *this,undefined4 param_1,int param_2)

{
  int iVar1;
  undefined4 *puVar2;
  
  FUN_0001d3f4(this);
  *(undefined ***)this = &PTR_LAB_0001ca88;
  *(undefined4 *)((int)this + 0x2c) = 0;
  *(undefined4 *)((int)this + 0x30) = 0;
  *(undefined4 *)((int)this + 0x34) = param_1;
  *(undefined4 *)((int)this + 0x38) = 0;
  *(undefined4 *)((int)this + 0x3c) = 0;
  FUN_0001dcd4((void *)((int)this + 0x40),1);
  FUN_0001dd1e((void *)((int)this + 0x6c),1,0);
  FUN_0001dd1e((void *)((int)this + 0x8c),1,0);
  FUN_00019b82((void *)((int)this + 0xac));
  *(uint *)((int)this + 0xc0) = *(uint *)((int)this + 0xc0) & 0xffffffc0;
  *(undefined4 *)((int)this + 0x108) = 0;
  *(uint *)((int)this + 0x10c) = *(uint *)((int)this + 0x10c) & 0xffffffc0;
  *(undefined4 *)((int)this + 0x1a4) = 0;
  *(undefined4 *)((int)this + 0x1a8) = 1;
  *(undefined4 *)((int)this + 0x1ac) = 0;
  FUN_0001dd1e((void *)((int)this + 0x1b0),1,0);
  FUN_0001a05e((undefined4 *)((int)this + 0x1d0));
  iVar1 = FUN_0001944c((int)this);
  *(int *)((int)this + 0x24) = iVar1;
  if ((param_2 != 0) && (-1 < iVar1)) {
    puVar2 = FUN_00019ee6((int)this);
    if (puVar2 == (undefined4 *)0x0) {
      *(undefined4 *)((int)this + 0x24) = 0xc000009a;
    }
  }
  return this;
}

