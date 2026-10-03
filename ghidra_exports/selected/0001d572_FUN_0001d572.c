
int FUN_0001d572(int param_1,undefined4 param_2,short *param_3,undefined4 param_4,short *param_5,
                uint param_6,uint param_7,int *param_8)

{
  int *piVar1;
  int iVar2;
  ushort *puVar3;
  wchar_t *pwVar4;
  undefined4 local_1c;
  int local_18;
  undefined1 local_14;
  ushort local_10 [2];
  short *local_c;
  undefined1 local_8;
  
  piVar1 = param_8;
  local_8 = 0;
  local_c = (short *)0x0;
  local_10[0] = 0;
  local_10[1] = 0;
  if ((param_3 == (short *)0xffffffff) || ((param_6 & 0x80) != 0)) {
    puVar3 = (ushort *)0x0;
    param_6 = param_6 | 0x80;
  }
  else if (param_3 == (short *)0x0) {
    puVar3 = (ushort *)0x0;
  }
  else {
    iVar2 = FUN_0001d418(local_10,L"\\Device\\",param_3,1);
    *piVar1 = iVar2;
    if (iVar2 < 0) goto LAB_0001d717;
    puVar3 = local_10;
  }
  iVar2 = IoCreateDevice(*(undefined4 *)(DAT_0001cdf8 + 4),param_2,puVar3,param_4,param_6,
                         param_7 >> 3 & 0xffffff01,&param_8);
  *piVar1 = iVar2;
  if (iVar2 < 0) goto LAB_0001d717;
  if (param_1 == 0) {
    param_1 = param_8[10];
  }
  else {
    param_8[10] = param_1;
  }
  *(int **)(param_1 + 4) = param_8;
  if (param_3 == (short *)0x0) {
LAB_0001d6c1:
    if ((param_5 == (short *)0x0) || (param_3 == (short *)0x0)) {
LAB_0001d725:
      param_8[7] = param_8[7] | param_7;
      iVar2 = *piVar1;
      *(undefined1 *)(param_1 + 0x28) = 0;
      *(int *)(param_1 + 0x24) = iVar2;
      FUN_0001d4d6();
      if (local_c == (short *)0x0) {
        return param_1;
      }
      FUN_000105cc(local_10);
      return param_1;
    }
    pwVar4 = L"\\DosDevices\\";
    if (DAT_0001ce08 == '\0') {
      pwVar4 = L"\\??\\";
    }
    iVar2 = FUN_0001d418((void *)(param_1 + 0x18),pwVar4,param_5,1);
    *piVar1 = iVar2;
    if (-1 < iVar2) {
      iVar2 = IoCreateSymbolicLink((void *)(param_1 + 0x18),param_1 + 0xc);
      *piVar1 = iVar2;
      if (-1 < iVar2) goto LAB_0001d725;
      IoDeleteDevice(param_8);
      *(undefined4 *)(param_1 + 4) = 0;
      goto LAB_0001d717;
    }
  }
  else if (param_3 == (short *)0xffffffff) {
    puVar3 = (ushort *)FUN_0001bcdc((int)param_8);
    if (puVar3 != (ushort *)0x0) {
      iVar2 = FUN_0001bda0((void *)(param_1 + 0xc),(*puVar3 >> 1) + 8,1);
      *piVar1 = iVar2;
      if (iVar2 < 0) goto LAB_0001d6b6;
      local_1c = *(undefined4 *)puVar3;
      local_14 = 0;
      local_18 = *(int *)(puVar3 + 2);
      FUN_0001bd6a((void *)(param_1 + 0xc),L"\\Device\\");
      RtlAppendUnicodeStringToString((void *)(param_1 + 0xc),&local_1c);
      if (local_18 != 0) {
        FUN_000105cc((undefined2 *)&local_1c);
      }
    }
LAB_0001d6b1:
    if (-1 < *piVar1) goto LAB_0001d6c1;
  }
  else {
    iVar2 = FUN_0001bda0((void *)(param_1 + 0xc),local_10[0] >> 1,1);
    *piVar1 = iVar2;
    if (-1 < iVar2) {
      FUN_0001bd6a((void *)(param_1 + 0xc),local_c);
      goto LAB_0001d6b1;
    }
  }
LAB_0001d6b6:
  IoDeleteDevice(param_8);
LAB_0001d717:
  if (local_c != (short *)0x0) {
    FUN_000105cc(local_10);
  }
  return 0;
}

