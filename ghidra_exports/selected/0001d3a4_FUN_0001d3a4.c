
void FUN_0001d3a4(int param_1)

{
  if (param_1 != 0) {
    FUN_0001d3a4(*(int *)(param_1 + 0xc));
    if (*(undefined4 **)(param_1 + 0x28) != (undefined4 *)0x0) {
      (**(code **)**(undefined4 **)(param_1 + 0x28))();
    }
  }
  return;
}

