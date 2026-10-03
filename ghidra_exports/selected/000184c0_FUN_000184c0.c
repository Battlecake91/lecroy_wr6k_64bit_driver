
undefined4 __thiscall FUN_000184c0(void *this,undefined4 param_1)

{
  undefined4 uVar1;
  undefined2 local_10 [2];
  int local_c;
  
  FUN_000105b4(local_10,param_1);
  uVar1 = RtlAppendUnicodeStringToString(this,local_10);
  if (local_c != 0) {
    FUN_000105cc(local_10);
  }
  return uVar1;
}

