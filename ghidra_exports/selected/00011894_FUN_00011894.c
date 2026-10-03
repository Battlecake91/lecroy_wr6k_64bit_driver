
void __thiscall FUN_00011894(void *this,int param_1)

{
  bool bVar1;
  
  bVar1 = FUN_0001bc0e(&param_1,0x1150a,0,(int *)(*(int *)((int)this + 4) + 0x14));
  if (bVar1) {
    if (**(char **)(param_1 + 0x60) == '\x0e') {
      *(undefined4 *)(param_1 + 0x1c) = 0;
      *(undefined4 *)(param_1 + 0x18) = 0xc0000010;
                    /* WARNING: Could not recover jumptable at 0x000118e0. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      (**(code **)(*(int *)((int)this + 0x1e0) + 0x20))();
      return;
    }
    (**(code **)(*(int *)((int)this + 0x1e0) + 0x20))(param_1);
  }
  return;
}

