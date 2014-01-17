// C implementation of Heap Sort
// Taken from: http://geeksquiz.com/heap-sort/
#include <stdio.h>
#include <stdlib.h>
 
// A heap has current size and array of elements
typedef struct maxheap_s {
   int size;
   int* array;
} maxheap_t;
 
/******************************************************************************/
static void swap(int* a, int* b) {
/******************************************************************************/
   int t = *a;
   *a = *b;
   *b = t;
}
 
/******************************************************************************/
void heapify(maxheap_t *heap, int idx)
/******************************************************************************/
// The main function to heapify a Max Heap.
// The function assumes that everything under given root (element at index idx)
// is already heapified.
{
    int g = idx;  // Initialize largest index as root
    int l = (idx << 1) + 1; // l = 2 * idx + 1
    int r = (idx + 1) << 1; // r = 2 * idx + 2
 
    // See if left child of root exists and it is greater than root
    if ((l < heap->size) && (heap->array[l] > heap->array[g])) {
        g = l;
    }
    // See if right child of root exists and is greater than the largest so far
    if ((r < heap->size) && (heap->array[r] > heap->array[g])) {
        g = r;
    }
    // Change root, if needed
    if (g != idx) {
        swap(&heap->array[g], &heap->array[idx]);
        heapify(heap, g);
    }
}
 
/******************************************************************************/
maxheap_t* create_heap(int *array, int len)
/******************************************************************************/
// A utility function to create a max heap of given capacity
{
    int i;
    maxheap_t* heap = (maxheap_t *)malloc(sizeof(maxheap_t));
    heap->size = len;   // Initialize size of heap
    heap->array = array; // Assign address of first element of array
 
    // Start from bottommost and rightmost internal mode and heapify all
    // internal modes in bottom up way
    for (i = (heap->size - 2) / 2; i >= 0; --i) {
        heapify(heap, i);
    }
    return heap;
}
 
/******************************************************************************/
static void heapsort(int* array, int len)
/******************************************************************************/
{
    // Build a heap from the input data.
    maxheap_t* heap = create_heap(array, len);
 
    // Repeat following steps while heap size is greater than 1. The last
    // element in max heap will be the minimum element
    while (heap->size > 1) {
       // The largest item in Heap is stored at the root. Replace it with the
       // last item of the heap followed by reducing the size of heap by 1.
        swap(&heap->array[0], &heap->array[heap->size - 1]);
        --heap->size;  // Reduce heap size
 
        // Finally, heapify the root of tree.
        heapify(heap, 0);
    }
}
 
/******************************************************************************/
int main(void)
/******************************************************************************/
{
   int i;
   int a[] = {12, 11, 13, 5, 6, 7};
   int n = sizeof(a) / sizeof(a[0]);
 
   printf("Original array (n=%d): ", n);
   for(i = 0; i < n; i++) {printf("%d ",a[i]);}
   printf("\n");

   heapsort(a, n);

   printf("Sorted array: ");
   for(i = 0; i < n; i++) {printf("%d ",a[i]);}
   printf("\n");
   return 0;
}

