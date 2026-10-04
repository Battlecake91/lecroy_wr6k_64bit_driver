
char * FUN_00018e58(void)

{
  uint in_EAX;
  
  if (0x17 < in_EAX) {
    return "<unknown minor function>";
  }
  return (&PTR_s_IRP_MN_START_DEVICE_0001cd88)[in_EAX];
}

