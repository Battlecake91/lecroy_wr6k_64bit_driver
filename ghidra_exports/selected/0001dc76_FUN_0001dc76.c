
void __thiscall FUN_0001dc76(void *this,uint *param_1,char param_2,int param_3)

{
  uint *puVar1;
  uint *puVar2;
  uint uVar3;
  uint *puVar4;
  uint uVar5;
  int local_8;
  
  uVar3 = 0;
  local_8 = 0;
  *(undefined4 *)this = 0;
  if (param_1 != (uint *)0x0) {
    puVar2 = param_1 + 1;
    if (*param_1 != 0) {
      do {
        puVar4 = puVar2 + 4;
        puVar1 = puVar2 + 3;
        uVar5 = 0;
        puVar2 = puVar4;
        if (*puVar1 != 0) {
          do {
            if ((char)*puVar4 == param_2) {
              if (local_8 == param_3) {
                *(uint **)this = puVar4;
                return;
              }
              local_8 = local_8 + 1;
            }
            puVar4 = puVar4 + 4;
            uVar5 = uVar5 + 1;
            puVar2 = puVar4;
          } while (uVar5 < *puVar1);
        }
        uVar3 = uVar3 + 1;
      } while (uVar3 < *param_1);
    }
  }
  return;
}

