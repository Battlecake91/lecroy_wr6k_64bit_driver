
void __fastcall FUN_000194ba(int param_1)

{
  *(uint *)(param_1 + 200) = *(uint *)(param_1 + 200) | 0x7f;
  *(uint *)(param_1 + 0xd8) = *(uint *)(param_1 + 0xd8) & 0xffffff83 | 3;
  *(uint *)(param_1 + 0xe8) = *(uint *)(param_1 + 0xe8) & 0xffffff95 | 0x15;
  *(uint *)(param_1 + 0xf8) = *(uint *)(param_1 + 0xf8) & 0xffffffdf | 0xdf;
  *(uint *)(param_1 + 0xfc) = *(uint *)(param_1 + 0xfc) | 0x1ff;
  *(uint *)(param_1 + 0x100) = *(uint *)(param_1 + 0x100) | 0x1ff;
  return;
}

