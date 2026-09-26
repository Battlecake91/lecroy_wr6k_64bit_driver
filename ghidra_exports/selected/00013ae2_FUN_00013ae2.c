
void __thiscall FUN_00013ae2(void *this,int param_1)

{
  void *this_00;
  void *this_01;
  bool bVar1;
  int iVar2;
  uint uVar3;
  undefined4 *puVar4;
  uint uVar5;
  undefined4 *puVar6;
  uint local_14;
  int local_10;
  int *local_c;
  ushort *local_8;
  
  if (DAT_0001cd08 == 0) {
    DAT_0001cd08 = 0x21;
    this_00 = (void *)((int)this + 0x116e);
    FUN_000153a2(this_00,*(uint *)(param_1 + 0xc),*(undefined4 *)(*(int *)(param_1 + 0x60) + 8));
    this_01 = (void *)((int)this + 0x118e);
    iVar2 = FUN_00015384((int)this_00);
    FUN_00015422(this_01,iVar2);
    local_8 = (ushort *)FUN_00015380((int)this_00);
    local_c = (int *)FUN_00015380((int)this_01);
    local_14 = 0;
    local_10 = 0;
    iVar2 = FUN_00015388((int)this_00);
    if (0 < iVar2) {
      do {
        if (local_c == (int *)0x0) {
LAB_00013c69:
          *(undefined4 *)(param_1 + 0x1c) = 0;
          *(undefined4 *)(param_1 + 0x18) = 0xc0000206;
          DAT_0001cd08 = DAT_0001cd08 & 0xffffffde;
          (**(code **)(*(int *)this + 0x20))(param_1);
          return;
        }
        bVar1 = FUN_00015710((int)local_8);
        if (bVar1) {
          FUN_00016c14((void *)((int)this + 0xea8),local_8,local_c);
          FUN_000154a6((int)this_01);
          *local_c = 0;
          *(undefined4 *)(param_1 + 0x18) = 0;
        }
        else {
          iVar2 = FUN_00015720((int)local_8);
          if ((char)iVar2 == '\0') goto LAB_00013c69;
          FUN_00012f30(this,(int)local_8);
        }
        local_14 = local_14 + *local_8;
        FUN_00015484(this_01,(uint)*local_8);
        FUN_00015364((int)this_00);
        local_8 = (ushort *)FUN_00015380((int)this_00);
        local_c = (int *)FUN_00015380((int)this_01);
        local_10 = local_10 + 1;
        iVar2 = FUN_00015388((int)this_00);
      } while (local_10 < iVar2);
    }
    uVar3 = local_14 & 0xffff;
    if (uVar3 <= *(uint *)(*(int *)(param_1 + 0x60) + 4)) {
      FUN_0001549e((int)this_01);
      FUN_00015380((int)this_01);
      puVar4 = (undefined4 *)FUN_00015380((int)this_01);
      puVar6 = *(undefined4 **)(param_1 + 0xc);
      for (uVar5 = uVar3 >> 2; uVar5 != 0; uVar5 = uVar5 - 1) {
        *puVar6 = *puVar4;
        puVar4 = puVar4 + 1;
        puVar6 = puVar6 + 1;
      }
      for (local_14 = local_14 & 3; local_14 != 0; local_14 = local_14 - 1) {
        *(undefined1 *)puVar6 = *(undefined1 *)puVar4;
        puVar4 = (undefined4 *)((int)puVar4 + 1);
        puVar6 = (undefined4 *)((int)puVar6 + 1);
      }
      *(uint *)(param_1 + 0x1c) = uVar3;
    }
    DAT_0001cd08 = DAT_0001cd08 & 0xffffffde;
  }
  else {
    *(undefined4 *)(param_1 + 0x1c) = 0;
    *(undefined4 *)(param_1 + 0x18) = 0xc00000a3;
  }
  return;
}

