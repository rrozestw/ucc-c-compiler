void f()
{
	goto a;

c: goto a;
b: goto c;
a: goto b;

//d: return;
}
