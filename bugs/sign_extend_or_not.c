unsigned short a[3];
long long foo()
{
  return a[0] | a[1]    << 16 | a[2]+0ull<<32;
}
long long bar()
{
  return a[0] | a[1]+0u << 16 | a[2]+0ull<<32;
}
