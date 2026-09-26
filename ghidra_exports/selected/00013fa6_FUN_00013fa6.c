
undefined1 __thiscall FUN_00013fa6(void *this,int param_1)

{
  char cVar1;
  undefined1 uVar2;
  void *pvVar3;
  int iVar4;
  char *pcVar5;
  uint uVar6;
  int iVar7;
  char *pcVar8;
  char *pcVar9;
  char *pcVar10;
  undefined4 *puVar11;
  bool bVar12;
  undefined4 auStackY_238 [61];
  undefined4 uStackY_144;
  char local_118 [256];
  undefined1 local_18;
  undefined4 local_17;
  undefined1 local_13;
  void *local_c;
  undefined1 local_5;
  
  iVar7 = param_1;
  local_5 = 0;
  local_c = this;
  if (param_1 == 0) {
    FUN_00018a1c((uint *)((int)this + 0x38),3,"InsertRegister: Null parameter\n");
    local_5 = 0;
  }
  else {
    iVar4 = FUN_00013f3c(this,param_1,&param_1);
    if (iVar4 == 0) {
      pcVar9 = *(char **)(iVar7 + 0xc);
      pcVar5 = local_118;
      for (iVar4 = 0x42; iVar4 != 0; iVar4 = iVar4 + -1) {
        pcVar5[0] = '\0';
        pcVar5[1] = '\0';
        pcVar5[2] = '\0';
        pcVar5[3] = '\0';
        pcVar5 = pcVar5 + 4;
      }
      pcVar5[0] = '\0';
      pcVar5[1] = '\0';
      pcVar5 = pcVar9;
      do {
        cVar1 = *pcVar5;
        pcVar5 = pcVar5 + 1;
      } while (cVar1 != '\0');
      pcVar8 = pcVar9;
      pcVar10 = local_118;
      for (uVar6 = (uint)((int)pcVar5 - (int)(pcVar9 + 1)) >> 2; uVar6 != 0; uVar6 = uVar6 - 1) {
        *(undefined4 *)pcVar10 = *(undefined4 *)pcVar8;
        pcVar8 = pcVar8 + 4;
        pcVar10 = pcVar10 + 4;
      }
      uVar2 = *(undefined1 *)(iVar7 + 0x20);
      for (uVar6 = (int)pcVar5 - (int)(pcVar9 + 1) & 3; pvVar3 = local_c, uVar6 != 0;
          uVar6 = uVar6 - 1) {
        *pcVar10 = *pcVar8;
        pcVar8 = pcVar8 + 1;
        pcVar10 = pcVar10 + 1;
      }
      pcVar9 = *(char **)(iVar7 + 0x10);
      iVar4 = 5;
      bVar12 = true;
      pcVar5 = pcVar9;
      pcVar8 = "BAR0";
      do {
        if (iVar4 == 0) break;
        iVar4 = iVar4 + -1;
        bVar12 = *pcVar5 == *pcVar8;
        pcVar5 = pcVar5 + 1;
        pcVar8 = pcVar8 + 1;
      } while (bVar12);
      if (bVar12) {
        local_18 = 0;
      }
      else {
        iVar4 = 5;
        bVar12 = true;
        pcVar5 = pcVar9;
        pcVar8 = "BAR1";
        do {
          if (iVar4 == 0) break;
          iVar4 = iVar4 + -1;
          bVar12 = *pcVar5 == *pcVar8;
          pcVar5 = pcVar5 + 1;
          pcVar8 = pcVar8 + 1;
        } while (bVar12);
        if (bVar12) {
          local_18 = 1;
        }
        else {
          iVar4 = 5;
          bVar12 = true;
          pcVar5 = pcVar9;
          pcVar8 = "BAR2";
          do {
            if (iVar4 == 0) break;
            iVar4 = iVar4 + -1;
            bVar12 = *pcVar5 == *pcVar8;
            pcVar5 = pcVar5 + 1;
            pcVar8 = pcVar8 + 1;
          } while (bVar12);
          if (bVar12) {
            local_18 = 2;
          }
          else {
            iVar4 = 5;
            bVar12 = true;
            pcVar5 = pcVar9;
            pcVar8 = "BAR3";
            do {
              if (iVar4 == 0) break;
              iVar4 = iVar4 + -1;
              bVar12 = *pcVar5 == *pcVar8;
              pcVar5 = pcVar5 + 1;
              pcVar8 = pcVar8 + 1;
            } while (bVar12);
            if (bVar12) {
              local_18 = 3;
            }
            else {
              iVar4 = 5;
              bVar12 = true;
              pcVar5 = pcVar9;
              pcVar8 = "BAR4";
              do {
                if (iVar4 == 0) break;
                iVar4 = iVar4 + -1;
                bVar12 = *pcVar5 == *pcVar8;
                pcVar5 = pcVar5 + 1;
                pcVar8 = pcVar8 + 1;
              } while (bVar12);
              if (bVar12) {
                local_18 = 4;
              }
              else {
                iVar4 = 5;
                bVar12 = true;
                pcVar5 = "BAR5";
                do {
                  if (iVar4 == 0) break;
                  iVar4 = iVar4 + -1;
                  bVar12 = *pcVar9 == *pcVar5;
                  pcVar9 = pcVar9 + 1;
                  pcVar5 = pcVar5 + 1;
                } while (bVar12);
                if (bVar12) {
                  local_18 = 5;
                }
              }
            }
          }
        }
      }
      local_17 = *(undefined4 *)(iVar7 + 0x1c);
      uStackY_144 = 0x140d9;
      local_13 = uVar2;
      FUN_00018a1c((uint *)((int)local_c + 0x38),1,
                   "InsertRegister: %s, BAR: %d, offset 0x%x, type %d\n");
      pcVar9 = local_118;
      puVar11 = auStackY_238;
      for (iVar7 = 0x42; iVar7 != 0; iVar7 = iVar7 + -1) {
        *puVar11 = *(undefined4 *)pcVar9;
        pcVar9 = pcVar9 + 4;
        puVar11 = puVar11 + 1;
      }
      *(undefined2 *)puVar11 = *(undefined2 *)pcVar9;
      FUN_00013f70((void *)((int)pvVar3 + 0x1c));
      local_5 = 1;
    }
    else {
      FUN_00018a1c((uint *)((int)this + 0x38),3,"Failed to insert KLecMemoryRegister\n");
    }
  }
  return local_5;
}

