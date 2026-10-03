
void __thiscall FUN_0001d7fa(void *this,ushort *param_1)

{
  ushort uVar1;
  void *_Dst;
  
  if ((*(int *)((int)this + 0xc) == 10) || (*(int *)((int)this + 0xc) == 0x10)) {
    uVar1 = *param_1;
    *(uint *)((int)this + 8) = (uint)(uVar1 >> 1);
    _Dst = (void *)FUN_0001039a((uint)(uVar1 >> 1) * 2 + 8,1);
    *(void **)this = _Dst;
    *(short *)((int)this + 0x12) = (*(short *)((int)this + 8) + 4) * 2;
    if (_Dst != (void *)0x0) {
      memmove(_Dst,*(void **)(param_1 + 2),*(int *)((int)this + 8) << 1);
      *(int *)((int)this + 4) = *(int *)((int)this + 4) + -1;
      FUN_0001d7a6(this);
    }
    RtlInitUnicodeString((int)this + 0x10,*(undefined4 *)this);
  }
  return;
}

