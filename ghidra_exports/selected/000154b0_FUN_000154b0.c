
void * __thiscall FUN_000154b0(void *this,uint param_1,undefined4 param_2)

{
  void *extraout_ECX;
  
  *(undefined4 *)((int)this + 0xc) = 0;
  *(undefined4 *)((int)this + 0x10) = 0;
  *(undefined4 *)((int)this + 8) = 0;
  *(undefined4 *)((int)this + 0x18) = 0;
  *(undefined4 *)((int)this + 0x14) = 0;
  *(undefined4 *)((int)this + 0x1c) = 0;
  if (param_1 != 0) {
    FUN_000153a2(this,param_1,param_2);
    this = extraout_ECX;
  }
  return this;
}

