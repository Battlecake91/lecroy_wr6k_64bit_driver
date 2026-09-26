
uint __thiscall FUN_000179e2(void *this,uint param_1)

{
  short *psVar1;
  
  psVar1 = (short *)((int)this + (param_1 >> 0x10 & 0xff) * 4 + 0x28);
  if (*psVar1 != (short)param_1) {
    *psVar1 = (short)param_1;
    WRITE_REGISTER_ULONG(*(undefined4 *)this,param_1);
  }
  return param_1;
}

