/*
 * Source code of simple merge sort implementation using array in ascending order
 * in c programming language
 * Taken from: http://geeksquiz.com/merge-sort/
 */
#include <stdio.h>
#include <stdlib.h>

#define ARRSIZE  12

void merge(int a[], int l, int m, int r);
void mergesort(int a[], int l, int r);

/******************************************************************************/
int main(void)
/******************************************************************************/
{
   int i, n = ARRSIZE;
   int mtable[ARRSIZE];

   /*Initialize the array*/
   for (i = 0; i < ARRSIZE; i++) {
      mtable[i] = rand() % ARRSIZE;
   }
   printf("Original array (n=%d): ", n);
   for(i = 0; i < n; i++) {printf("%d ",mtable[i]);}
   printf("\n");

   mergesort(mtable, 0, n - 1);

   printf("Sorted array: ");
   for(i = 0; i < n; i++) {printf("%d ",mtable[i]);}
   printf("\n");
   return 0;
}

/******************************************************************************/
void mergesort(int a[], int l, int r)
/******************************************************************************/
{
    int m; /*Middle element*/

    if (l < r) {
         m = (l + r) / 2;
         mergesort(a, l, m);
         mergesort(a, m + 1, r);
         merge(a, l, m, r);
    }
}

/******************************************************************************/
void merge(int a[], int l, int m, int r)
/******************************************************************************/
{
   int i, j, k;
   int n1 = m - l + 1;
   int n2 =  r - m;
   int la[n1], ra[n2]; /*Temp arrays*/
 
   /* Copy data to temp arrays la[] and ra[] */
   for (i = 0; i < n1; i++) {
      la[i] = a[l + i];
   }
   for (j = 0; j <= n2; j++) {
      ra[j] = a[m + 1 + j];
   }
   /*Merge the temp arrays back into a[l..r]*/
   i = 0;  j = 0;  k = l;
   while (i < n1 && j < n2) {
      if (la[i] <= ra[j]) {
         a[k] = la[i];
         i++;
      }
      else {
         a[k] = ra[j];
         j++;
      }
      k++;
   }
   /*Copy the remaining elements of la[], if there are any*/
   while (i < n1) {
      a[k] = la[i];
      i++;
      k++;
   }
   /*Copy the remaining elements of ra[], if there are any */
   while (j < n2) {
      a[k] = ra[j];
      j++;
      k++;
   }
}

