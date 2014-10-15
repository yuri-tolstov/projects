#!/usr/bin/python

def askok(prompt, retries = 4, comp = 'Yes or No'):
   while True:
       ok = raw_input(prompt)
       if ok in ('y', 'Y', 'yes'):
           return True
       if ok in ('n', 'N', 'no'):
           return False

       retries = retries - 1
       if retries < 0:
           raise IOError('Refusink User')

       print comp

# Program:
print askok('Quit? ', 2)

