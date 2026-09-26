
void __fastcall FUN_00016544(int param_1)

{
  *(undefined1 *)(param_1 + 0x23) = 0;
  if ((*(short *)(param_1 + 0x21) != 0) && (*(int *)(param_1 + 0x1d) != 0)) {
    ExFreePool(*(int *)(param_1 + 0x1d));
    *(undefined4 *)(param_1 + 0x1d) = 0;
    *(undefined2 *)(param_1 + 0x21) = 0;
  }
  *(undefined4 *)(param_1 + 0x182) = 0;
  if (*(void **)(param_1 + 0x186) != (void *)0x0) {
    FUN_00016524(*(void **)(param_1 + 0x186),1);
  }
  FUN_0001a07c();
  FUN_000156d2(param_1 + 0x31);
  return;
}

