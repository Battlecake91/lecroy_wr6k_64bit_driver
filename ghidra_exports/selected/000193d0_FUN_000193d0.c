
void __thiscall
FUN_000193d0(void *this,int param_1,undefined4 param_2,char param_3,char param_4,char param_5)

{
  int iVar1;
  byte *pbVar2;
  
  if (param_1 == 0) {
    param_5 = '\0';
    param_4 = '\0';
    param_3 = '\0';
  }
  iVar1 = *(int *)(*(int *)this + 0x60);
  *(int *)(iVar1 + -8) = param_1;
  *(undefined4 *)(iVar1 + -4) = param_2;
  pbVar2 = (byte *)(iVar1 + -0x21);
  *pbVar2 = 0;
  if (param_3 != '\0') {
    *pbVar2 = 0x40;
  }
  if (param_4 != '\0') {
    *pbVar2 = *pbVar2 | 0x80;
  }
  if (param_5 != '\0') {
    *pbVar2 = *pbVar2 | 0x20;
  }
  return;
}

