
void __fastcall FUN_000133c4(undefined4 *param_1)

{
  FUN_00018a1c(param_1 + 7,1,"~CKeTraceControl()\n");
  if (param_1[0x12] != 0) {
    ExFreePool(param_1[0x12]);
    param_1[0x12] = 0;
  }
  FUN_00018b9e((int)(param_1 + 7));
  if ((param_1[4] == 0) && (param_1[1] == 0)) {
    return;
  }
  FUN_00012166(param_1);
  return;
}

