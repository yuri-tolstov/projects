/* A typical recursive implementation of quick sort.
 * Taken from: http://geeksquiz.com/quick-sort/
 */
#include<stdio.h>
 
/******************************************************************************/
void swap(int *a, int *b)
/******************************************************************************/
{
   int t = *a;
   *a = *b;
   *b = t;
}
 
/******************************************************************************/
static int partition (int a[], int l, int r)
/******************************************************************************/
/* This function takes last element as pivot, places the pivot element at its
 * correct position in sorted array, and places all smaller (smaller than pivot)
 * to left of pivot and all greater elements to right of pivot.
 */
{
   int x = a[r];   //Pivot
   int i = l - 1;  //Index of smaller element
 
   for (int j = l; j <= r - 1; j++) {
      //If current element is smaller than or equal to pivot
      if (a[j] <= x) {
         i++;    //Increment index of smaller element
         swap(&a[i], &a[j]);  //Swap current element with index
      }
   }
   swap(&a[i + 1], &a[r]); 
   return i + 1;
}
 
/******************************************************************************/
static void quicksort(int a[], int l, int r)
/******************************************************************************/
{
   if (l < r) {
      int p = partition(a, l, r); /* Partitioning index */
      quicksort(a, l, p - 1);
      quicksort(a, p + 1, r);
   }
}
 
/******************************************************************************/
int main(void)
/******************************************************************************/
{
   int i;
   int a[] = {10, 7, 8, 9, 1, 5};
   int n = sizeof(a) / sizeof(a[0]);

   printf("Original array (n=%d): ", n);
   for(i = 0; i < n; i++) {printf("%d ",a[i]);}
   printf("\n");

   quicksort(a, 0, n - 1);

   printf("Sorted array: ");
   for(i = 0; i < n; i++) {printf("%d ",a[i]);}
   printf("\n");
   return 0;
}

