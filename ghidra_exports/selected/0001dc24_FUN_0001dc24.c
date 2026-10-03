
undefined4 * __thiscall
FUN_0001dc24(void *this,int param_1,undefined4 param_2,char param_3,undefined4 param_4,
            undefined4 param_5,undefined4 param_6)

{
  wchar_t *pwVar1;
  ushort *puVar2;
  undefined1 local_14 [8];
  ushort local_c [4];
  
  *(undefined4 *)this = 0;
  *(undefined4 *)((int)this + 4) = 0;
  *(undefined4 *)((int)this + 8) = 0;
  *(undefined4 *)((int)this + 0xc) = 0;
  *(undefined4 *)((int)this + 0x10) = 0;
  RtlInitUnicodeString(local_c,param_2);
  puVar2 = local_c;
  pwVar1 = FUN_0001dbce(local_14,param_1);
  FUN_0001d8a2(this,(ushort *)pwVar1,puVar2,param_5,param_6,param_3,param_4);
  return this;
}

