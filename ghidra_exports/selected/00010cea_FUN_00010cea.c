
void __thiscall FUN_00010cea(int param_1,int param_2)

{
  *(char *)(param_2 + 0x23) = *(char *)(param_2 + 0x23) + '\x01';
  *(int *)(param_2 + 0x60) = *(int *)(param_2 + 0x60) + 0x24;
  FUN_000107e0(param_1);
  return;
}

