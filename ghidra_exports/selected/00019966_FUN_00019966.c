
void FUN_00019966(int param_1,undefined4 param_2,undefined4 param_3,int param_4,undefined4 param_5)

{
  if (param_4 != 0) {
    if (*(code **)(param_4 + 8) != (code *)0x0) {
      (**(code **)(param_4 + 8))(*(undefined4 *)(param_1 + 4),param_2,param_3,param_4,param_5);
    }
    if (*(int *)(param_4 + 4) != 0) {
      KeSetEvent(*(undefined4 *)(*(int *)(param_4 + 4) + 4),0,0);
    }
    ExFreePool(param_4);
  }
  return;
}

