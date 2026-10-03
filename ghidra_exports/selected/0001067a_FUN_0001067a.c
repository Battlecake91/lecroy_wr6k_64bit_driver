
undefined4 __thiscall FUN_0001067a(void *this,int param_1,undefined4 param_2,undefined4 param_3)

{
  byte *pbVar1;
  
  pbVar1 = (byte *)(*(int *)(param_1 + 0x60) + 3);
  *pbVar1 = *pbVar1 | 1;
  IoStartPacket(*(undefined4 *)((int)this + 4),param_1,param_3,param_2);
  return 0x103;
}

