// RUN: %check -e %s
main()
{
	switch(2.1){ // CHECK: error: switch requires an integral type (not "double")
	}
}
