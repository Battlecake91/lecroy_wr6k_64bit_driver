
char * FUN_00018edb(void)

{
  uint in_EAX;
  
  if (3 < in_EAX) {
    return "<unknown minor function>";
  }
  return (&PTR_s_IRP_MN_WAIT_WAKE_0001cde8)[in_EAX];
}

