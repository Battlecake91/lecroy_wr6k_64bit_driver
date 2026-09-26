
int __thiscall FUN_00018194(void *this,int param_1,int param_2,undefined4 *param_3)

{
  undefined4 *puVar1;
  int iVar2;
  uint uVar3;
  uint uVar4;
  int iVar5;
  undefined4 *puVar6;
  int local_1c;
  int *local_18;
  int local_14;
  int local_10;
  uint local_c;
  int local_8;
  
  local_10 = 0;
  local_14 = 0;
  local_8 = 0;
  puVar6 = param_3;
  do {
    if (param_3 == (undefined4 *)0x0) {
      puVar6 = (undefined4 *)(param_1 + 8 + (int)puVar6 * 8);
      *puVar6 = 0;
      puVar6[1] = 0;
      return local_8;
    }
    local_c = param_3[5];
    iVar5 = param_3[6];
    local_18 = param_3 + 7;
    if (local_c != 0) {
      local_1c = local_10 << 9;
      do {
        iVar2 = *local_18;
        uVar3 = 0x1000U - iVar5;
        if (local_c < 0x1000U - iVar5) {
          uVar3 = local_c;
        }
        puVar6 = (undefined4 *)(local_1c + local_14);
        if (local_14 == 0x1ff) {
          local_14 = 0;
          puVar1 = (undefined4 *)(param_1 + (int)puVar6 * 8);
          *puVar1 = 0;
          local_10 = local_10 + 1;
          local_1c = local_1c + 0x200;
          puVar1[1] = *(int *)(param_2 + 0x1c + local_10 * 4) << 0xc;
          puVar6 = (undefined4 *)((int)puVar6 + 1);
        }
        uVar4 = uVar3 >> 2;
        if (*(uint *)((int)this + 0xc) <= local_8 + uVar4) {
          uVar4 = *(uint *)((int)this + 0xc) - local_8;
        }
        local_8 = local_8 + uVar4;
        local_14 = local_14 + 1;
        *(uint *)(param_1 + (int)puVar6 * 8) = uVar4;
        *(int *)(param_1 + 4 + (int)puVar6 * 8) = iVar2 * 0x1000 + iVar5;
        if (local_8 == *(int *)((int)this + 0xc)) break;
        local_c = local_c - uVar3;
        local_18 = local_18 + 1;
        iVar5 = 0;
      } while (local_c != 0);
    }
    param_3 = (undefined4 *)*param_3;
  } while( true );
}

