// RUN: %check -e %s
typedef unsigned long size_t;
typedef int *size_t; // CHECK: /error: redefinition of size_t from:/
