// RUN: %ucc -S %s; [ $? -ne 0 ]
// RUN: %check %s

// this is a bad constant cast
int x;
int *p = (int)&x; // CHECK: /global scalar initialiser not constant/
