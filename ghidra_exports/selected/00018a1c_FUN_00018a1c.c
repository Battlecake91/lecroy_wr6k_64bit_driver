
void __cdecl FUN_00018a1c(uint *param_1,int param_2,char *param_3)

{
  char cVar1;
  LONG LVar2;
  char *_Str;
  char *pcVar3;
  char *pcVar4;
  uint uVar5;
  uint uVar6;
  LONG *unaff_EBX;
  LONG *unaff_ESI;
  char *pcVar7;
  LONG *lpAddend;
  int local_8;
  
  LVar2 = InterlockedDecrement(unaff_ESI);
  if (LVar2 < 0) {
    InterlockedIncrement(lpAddend);
    DbgPrint("KTrace: Dropping a trace message due to lack of buffer space\n");
  }
  else {
    if (((param_2 == 0) || ((int)param_1[1] <= param_2)) && (param_2 != 5)) {
      _Str = &DAT_0001ce50 + LVar2 * 0x100;
      pcVar3 = _Str;
      local_8 = _vsnprintf(_Str + param_1[4],0x100 - param_1[4],param_3,&stack0x00000010);
      if (local_8 == -1) {
        (&DAT_0001cf4f)[LVar2 * 0x100] = 0;
        local_8 = 0x100 - param_1[4];
      }
      if ((char *)param_1[3] != (char *)0x0) {
        uVar6 = param_1[4];
        pcVar4 = (char *)param_1[3];
        for (uVar5 = uVar6 >> 2; uVar5 != 0; uVar5 = uVar5 - 1) {
          *(undefined4 *)_Str = *(undefined4 *)pcVar4;
          pcVar4 = pcVar4 + 4;
          _Str = _Str + 4;
        }
        pcVar7 = _Str;
        for (uVar6 = uVar6 & 3; _Str = pcVar3, uVar6 != 0; uVar6 = uVar6 - 1) {
          *pcVar7 = *pcVar4;
          pcVar4 = pcVar4 + 1;
          pcVar7 = pcVar7 + 1;
        }
      }
      uVar6 = *param_1;
      if ((uVar6 & 2) == 0) {
        if ((DAT_0001d254 != (code *)0x0) && ((uVar6 & 1) != 0)) {
          if ((char)param_1[6] == '\0') {
            uVar6 = param_1[4];
          }
          else {
            uVar6 = 0;
          }
          (*DAT_0001d254)(param_1[5],_Str + uVar6);
        }
      }
      else {
        if ((uVar6 & 4) != 0) {
          for (pcVar3 = strchr(_Str,0x25); pcVar3 != (char *)0x0; pcVar3 = strchr(pcVar3 + 2,0x25))
          {
            pcVar4 = pcVar3;
            do {
              cVar1 = *pcVar4;
              pcVar4 = pcVar4 + 1;
            } while (cVar1 != '\0');
            pcVar4 = pcVar4 + (1 - (int)(pcVar3 + 1));
            if ((char *)0x100 < pcVar3 + ((int)pcVar4 - (int)_Str)) {
              pcVar4 = _Str + (0xff - (int)pcVar3);
              _Str[0xff] = '\0';
            }
            memmove(pcVar3 + 1,pcVar3,(size_t)pcVar4);
          }
        }
        if ((char)param_1[6] == '\0') {
          uVar6 = param_1[4];
        }
        else {
          uVar6 = 0;
        }
        DbgPrint(_Str + uVar6);
      }
      *(bool *)(param_1 + 6) = _Str[param_1[4] + local_8 + -1] == '\n';
    }
    if (((param_2 == 0) || ((int)param_1[2] <= param_2)) && (param_1[2] != 5)) {
      DbgBreakPoint();
    }
    InterlockedIncrement(unaff_EBX);
  }
  return;
}

