
uint __thiscall FUN_00016962(void *this,int param_1)

{
  undefined2 uVar1;
  byte bVar2;
  uint uVar3;
  
  uVar1 = *(undefined2 *)(param_1 + 1);
  *(undefined2 *)((int)this + 0xb) = uVar1;
  bVar2 = (byte)((ushort)uVar1 >> 8);
  FUN_00016074(this,(byte)uVar1 >> 7);
  FUN_0001573e(this,bVar2 >> 3 & 1);
  uVar3 = FUN_00015772(this,bVar2 & 1);
  return uVar3 & 0xffff0000;
}

