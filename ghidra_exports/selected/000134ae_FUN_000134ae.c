
void __fastcall FUN_000134ae(undefined4 *param_1)

{
  FUN_00018a1c(param_1 + 0xe,1,"~CKeRegisterList()\n");
  FUN_00018b9e((int)(param_1 + 0xe));
  if ((param_1[0xb] != 0) || (param_1[8] != 0)) {
    FUN_00012166(param_1 + 7);
  }
  if ((param_1[4] == 0) && (param_1[1] == 0)) {
    return;
  }
  FUN_00012166(param_1);
  return;
}

