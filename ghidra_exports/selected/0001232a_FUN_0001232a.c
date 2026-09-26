
uint __thiscall FUN_0001232a(void *this,int param_1,uint param_2)

{
  uint in_EAX;
  uint uVar1;
  int *piVar2;
  
  uVar1 = in_EAX & 0xffffff00;
  for (piVar2 = *(int **)this; piVar2 != (int *)0x0; piVar2 = (int *)piVar2[3]) {
    if (*piVar2 == param_1) {
      piVar2[2] = piVar2[2] | param_2;
      uVar1 = CONCAT31((int3)(param_2 >> 8),1);
    }
  }
  return uVar1;
}

