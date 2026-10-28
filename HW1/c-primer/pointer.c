// Copyright (c) 2012 MIT License by 6.172 Staff

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char * argv[]) {  // What is the type of argv? A pointer to a pointer
  int i = 5;
  // The & operator here gets the address of i and stores it into pi
  int * pi = &i;
  // The * operator here dereferences pi and stores the value -- 5 --
  // into j.
  int j = *pi;

  char c[] = "6.172";
  char * pc = c;  // Valid assignment: c acts like a pointer to c[0] here.
  char d = *pc;
  printf("char d = %c\n", d);  // What does this print? 6

  // compound types are read right to left in C.
  // pcp is a pointer to a pointer to a char, meaning that
  // pcp stores the address of a char pointer.
  char ** pcp;
  pcp = argv;  // Why is this assignment valid? Because argv is a pointer to first element in the argv[] array which is an array of pointers

  const char * pcc = c;  // pcc is a pointer to char constant
  char const * pcc2 = c;  // What is the type of pcc2? A pointer to a const char

  // For each of the following, why is the assignment:
  //*pcc = '7';  // invalid? Because the data the pointer points to is constant
  pcc = *pcp;  // valid? Because the pointer itself isn't constant
  pcc = argv[0];  // valid? Same idea. The first element in argv is a pointer.

  char * const cp = c;  // cp is a const pointer to char
  // For each of the following, why is the assignment:
  //cp = *pcp;  // invalid? Because the pointer here is constant
  //cp = *argv;  // invalid? Saem idea. *argv is the first pointer in the array of pointers
  *cp = '!';  // valid? Because the data itself isn't constant

  const char * const cpc = c;  // cpc is a const pointer to char const
  // For each of the following, why is the assignment:
  //cpc = *pcp;  // invalid? The pointer is constant (pcp is a double pointer)
  //cpc = argv[0];  // invalid? The pointer itself is constant
  //*cpc = '@';  // invalid? The data pointed to by the pointer is constant

  return 0;
}
