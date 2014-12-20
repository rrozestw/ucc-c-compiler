f()
{
	if(g())
		a();
	else
		b();

	goto a;

c: goto a;
b: goto c;
a: goto b;

d: return;
}
