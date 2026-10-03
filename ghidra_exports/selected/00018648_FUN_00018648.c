
void __fastcall FUN_00018648(int param_1)

{
  if (*(int *)(param_1 + 4) != 0) {
    IoDisconnectInterrupt(*(int *)(param_1 + 4));
    *(undefined4 *)(param_1 + 4) = 0;
  }
  return;
}

