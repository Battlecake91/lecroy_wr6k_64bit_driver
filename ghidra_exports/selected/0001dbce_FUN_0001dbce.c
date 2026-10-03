
wchar_t * FUN_0001dbce(undefined4 param_1,int param_2)

{
  wchar_t *pwVar1;
  
  if (param_2 == 0) {
    pwVar1 = L"";
  }
  else if (param_2 == 1) {
    pwVar1 = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services";
  }
  else if (param_2 == 2) {
    pwVar1 = L"\\Registry\\Machine\\System\\CurrentControlSet\\Control";
  }
  else if (param_2 == 3) {
    pwVar1 = L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion";
  }
  else if (param_2 == 4) {
    pwVar1 = L"\\REGISTRY\\Machine\\HARDWARE\\DEVICEMAP";
  }
  else {
    if (param_2 != 5) {
      return (wchar_t *)0x0;
    }
    pwVar1 = L"\\Registry\\User";
  }
  RtlInitUnicodeString(param_1);
  return pwVar1;
}

