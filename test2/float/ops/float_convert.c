// RUN: [ `%ucc %s -S -o- -fno-inline-functions | grep cvtss2sd | wc -l` -eq 1 ]
// RUN: [ `%ucc %s -S -o- -fno-inline-functions | grep cvtsd2ss | wc -l` -eq 1 ]

take_f(float f);

take_d(double d)
{
	take_f(d);
}

take_f(float f)
{
	take_d(f);
}
