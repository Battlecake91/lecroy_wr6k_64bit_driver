
void __fastcall FUN_0001d7a6(int *param_1)

{
  undefined4 local_c;
  int local_8;
  
  if (*param_1 != 0) {
    param_1[1] = param_1[1] + 1;
    local_8 = *param_1 + param_1[2] * 2;
    local_c = 0x80000;
    RtlIntegerToUnicodeString(param_1[1],param_1[3],&local_c);
    *(undefined2 *)(local_8 + (uint)((ushort)local_c >> 1) * 2) = 0;
    *(ushort *)(param_1 + 4) = (short)param_1[2] + (ushort)local_c;
  }
  return;
}

