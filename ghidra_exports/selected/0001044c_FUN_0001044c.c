
void FUN_0001044c(int param_1,int param_2)

{
  if (DAT_0001cdfc == '\0') {
    (**(code **)(**(int **)(param_1 + 0x28) + 8))(param_2);
  }
  else {
    (**(code **)(*DAT_0001cdf8 + 0x10))
              (*(undefined4 *)(param_1 + 0x28),param_2,
               (&PTR_LAB_0001cd10)[**(byte **)(param_2 + 0x60)]);
  }
  return;
}

