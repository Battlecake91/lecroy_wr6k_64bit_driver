
void __thiscall FUN_0001259a(void *this,int param_1)

{
  uint *puVar1;
  
  puVar1 = (uint *)((int)this + 0x38);
  FUN_00018a1c(puVar1,1,"SetOneRegister() \n");
  if (param_1 == 0) {
    FUN_00018a1c(puVar1,3,"SetOneRegister: Null parameter\n");
  }
  else {
    FUN_000107fe(*(void **)(*(int *)((int)this + 0x10) + *(int *)(param_1 + 0x101) * 4),
                 *(undefined4 *)(param_1 + 0x106));
    FUN_00018a1c(puVar1,1,"index[0x%2d]: %s, BAR: %d, offset 0x%x, type %d, data: 0x%x\n");
  }
  return;
}

