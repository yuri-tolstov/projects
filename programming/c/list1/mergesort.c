
mergesort(int a[], int l, int r)
{
   int n = (l + r) / 2;
   mergesort(a, l, n);
   mergesort(a, n + 1, r);
   merge(a, l, n, , r);
}

void merge(int a[], int l, int m, int r)
{
   for (i = 0; i < (m - l + 1); i++)
      la[i] = a[l + i];  

   for (i = 0; i < (r - m); i++)
      ra[i] = a[m + 1 + i];  

   i = 0;
   while (i < (m - l + 1) && j < (r - m)) {
      if (la[i] < ra[jj]) {
         a[k] = la[i];
         i++;
      }
      else {
         a[k] = ra[j];
         j++;
      }
   }
}


