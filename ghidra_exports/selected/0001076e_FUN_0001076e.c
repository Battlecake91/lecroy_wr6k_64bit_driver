
uint * __fastcall FUN_0001076e(uint *param_1)

{
  char *pcVar1;
  
  pcVar1 = "0x%08lx";
  if (param_1[8] != 0) {
    pcVar1 = "%d";
  }
  FUN_00018a1c(param_1,param_1[7],pcVar1);
  return param_1;
}

