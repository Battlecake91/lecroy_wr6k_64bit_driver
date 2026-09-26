
uint __thiscall FUN_00015fd8(void *this,int param_1)

{
  uint uVar1;
  
  if ((param_1 == 0) || (*(void **)((int)this + 0x17a) == (void *)0x0)) {
    uVar1 = CONCAT22((short)((uint)param_1 >> 0x10),8);
  }
  else {
    FUN_000107fe(*(void **)((int)this + 0x17a),(uint)*(ushort *)(param_1 + 1));
    uVar1 = FUN_00015a88(this,0);
    uVar1 = uVar1 & 0xffff0000;
  }
  return uVar1;
}

