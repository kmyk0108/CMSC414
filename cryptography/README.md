## General Description

In this project, you will be attacking a weak ciphersuite in an online
bank. The bank is using a secure **128-bit block cipher**, with a
*unique key* per session. To make a streaming cipher from this, the
bank is using the cipher in ECB mode.

As an attacker, you were able to inspect the ciphertext stream sent by
you to the bank's server. Due to some technical limitations, you were
unable to capture the server's responses, and you were only able to do
this once.  However, you know exactly what operations you performed,
and in what order, so you should be able to figure out the encrypted
message formats, despite *not knowing the plaintext format*. That is,
the *only* things you have are what you entered into the bank's
website, and the corresponding ciphertext stream.

From this, you will need to do the following:

 * Learn the format of the messages
 * Write a program to parse these messages and generate new messages
 * Perform a number of passive- and active-wiretapping attacks on
   the session of *another* customer (the target), which uses a
   *different* key

**Please note that you will _not_ be decrypting the streams!** Assume the
cryptosystem is unbreakable.


## The Reference Streams

Ciphertext streams can be found on ELMS in "Cryptography Sessions"
under "Files".  The file `reference.in` contains the following
sequence of requests, in the order given:

 1. A balance request for your checking account
 2. A $100 transfer from your checking account to your savings
    account, to be executed immediately
 3. A balance request for your checking account
 4. A balance request for your savings account
 5. A $1000 transfer from your savings account to your checking
    account, to be executed immediately
 6. A $1000 transfer from your savings account to your checking
    account, to be executed tomorrow
 7. A balance request for your savings account
 8. An invoice request issued by your checking account for payment
    of $1000 from your savings account
 9. A balance request for your checking account

You are also given additional ciphertext streams, which are from
*different* sessions, so they are not encrypted with the same key.
In addition, you do *not* know what requests were used to generate
this.  This will allow you to test your code against an unknown
request sequence.


## Tasks

There are four tasks, and each will require a separate executable.
For instance, the executable for task 1 will be named `task1`.  Note,
this should ***not*** be `task1.x`, `task1.exe`, `task1.sh`,
`task1.py`, or any other file extension.  If this executable must be
compiled, you must provide a `Makefile` to do the compilation, which
will be called *without arguments*. That is, do not expect us to run
`make task1`.

Each task will operate on a *separate session*. That is, they will all
be encrypted with different keys. You will have limited information
about each session. We will guarantee the following about sessions:

 * **No transaction will have the same source and destination account.**
 * **Except where noted in the task description, all account numbers will
   appear at least twice.**
 * **All three transaction types will appear.**
 * **At a minimum, there will be one repeated BALANCE request transaction.**
 * **There are no partial transactions, so every transaction that begins
   in the session will be complete.**

See the Implementation Notes below for formatting of output and other
requirements.

### Task 1

For this task, you will provide an executable named `task1` that reads
the ciphertext for a session and outputs

 * The types of messages, in order, with each on a separate line

The ciphertext will be provided as the only command-line argument to
your executable, and will be in the same format as the reference
stream. That is, we will call it as (for an input file `task1.in`)
```
./task1 task1.in
```

### Task 2

For this task, you will provide an executable named `task2` that reads
the ciphertext for a session and outputs

 * The types of messages, in order, with each on a separate line
 * A replay (unmodified copy) of a message that transfers money
   into your account, written to a file named `task2.out`

For this session, you know that *exactly one* message includes your
account, and it is transfering money to you. The file `task2.out`
should include the **entire** stream, with your replay added to it.
That is, there should be exactly one more message in `task2.out`
than the input stream.

As before, the input ciphertext stream will be provided as the only
command-line argument to your executable, and will be in the same
format as other streams.

### Task 3

For this task, you will provide an executable named `task3` that reads
the ciphertext for a session and outputs

 * The types of messages, in order, with each on a separate line
 * A modified money transfer to your account, where the amount in the
   transfer is changed to a valid new value, written to a file
   `task3.out`

For this session, you know that the target sent a money transfer
to your account for $10. No other requests involving your account
are in this session. In addition, the target requested payments
(invoices) from at least one other account.

Your executable should produce a file `task3.out` containing the
input ciphertext with the modified message. That is, `task3.out`
should contain the same messages as the input, but with the amount
changed in the payment to your account, so the target is sending
you a different amount of money.

As before, the input ciphertext stream will be provided as the only
command-line argument to your executable, and will be in the same
format as other streams.

### Task 4

For this task, you will provide an executable named `task4` that reads
the ciphertext for a session and outputs

 * The types of messages, in order, with each on a separate line
 * A money transfer to your account *instead of* a payment request
   from your account, written to a file `task4.out`

For this session, you know that the target requested payment from your
account, and this is the only request involving your account. You must
convert this payment request *from* your account into a money transfer
*to* your account.

Your executable should produce a file `task4.out` containing the input
ciphertext with the modified message. That is, `task4.out` should
contain the same messages as the input, but with the request for
payment from you changed to a transfer to you.

As before, the input ciphertext stream will be provided as the only
command-line argument to your executable, and will be in the same
format as other streams.

## Implementation Notes

The required output must match what we have asked for **exactly**.
Anything not part of the required output should be printed to standard
error, not standard output.

The following table shows the expected way to print message types:

| **Message Type**    | **Value to Print** |
| ------------------- | ------------------ |
| balance request     | BALANCE            |
| money transfer      | TRANSFER           |
| request for payment | INVOICE            |


## Submission

You must commit your submission to git.  Do not add compiled files
to git, only scripts, source code, and (if needed) a `Makefile`.

## Scoring

 * Identifying all messages correctly will cumulatively be worth 20
   points (tasks 1-4)
 * Producing valid messages will cumulatively be worth 15 points
   (tasks 2-4)
 * Correctly modifying a messages will cumulatively be worth 10 points
   (tasks 3 and 4)
 * Correctly changing the type of a message will be worth 5 points
   (task 4)

## Tips

### Parsing the Session Structure

There is no limit on the number of accounts showing up in a
session. It might be useful to think of each account as a principal in
the system (which it is). You can consider all of the accounts
originating transactions as being principals that share a single
identity. The attacker, in contrast, is a separate identity, and will
not be initiating a transaction in tasks 2-4.

You should be able to start by assuming the first transaction is
of a particular type, and see what that implies for the next
transaction, which will either be the same type or one of the other
two. You can do this iteratively, and ultimately you’ll see one of
the following:

  1. You have consumed the entire session, with only three transaction
     types appearing.
  2. You have found some number of transaction types other than three.
  3. You either have bytes left in the session that cannot form a new
     transaction, or your final transaction is incomplete.

The first of these should indicate successful parsing. Either of the
others means you have made an incorrect assumption/guess during your
parsing, and you need to unwind. Make sure you keep track of what
ciphertext corresponds to what type of field as you parse --- no
two fields of different types will encrypt to the same ciphertext.

You should be able to enumerate all of the possibilities in your code,
since there are only six ways to order three message types by the order
in which they *first* occur.

### Examining Binary Files

You are strongly encouraged to view the ciphertext streams through a
hex-formatting program like `xxd` or `hexdump`.

### Common Code

While you must provide four separate executables, you will probably
want to have some code in common between them. This might be an
additional `.c` and `.h` file, a python file to import, or something
else. You might even have a single binary, and bash scripts to call it
with appropriate arguments for the individual tasks.

### Using C

C is a good language for working with binary data, though you are free
to use any language already installed in the `baseline` image. We
*will not* install additional packages for you, and you *should not*
assume Internet access when building or running your code.

If you would like to use C, here are some things you might find
useful.

#### Writing to STDERR

Since we're expecting very specific output in STDOUT, all of your
debugging output should go to STDERR. You can do this with:
```C
fprintf(stderr,"This is an error message\n");
```

You might also find the following useful:
```C
fprintf(stderr,"%s: %d\n",__FILE__,__LINE__);
```
The preprocessor will replace `__FILE__` with the current file name,
and `__LINE__` with the current line of the file. That means you
can add this exact line anywhere you like in your code to trace the
program flow. This can be extremely helpful when you're trying to
track down where a segfault is occurring.

#### Working with Binary Data

Be careful when working with ciphertext that you *do not* use string
functions. Instead, you should use `memcpy` to copy data from one
binary array to another, and `memcmp` to compare arrays. See the
documentation for both of these.

#### Linked Lists

C does not have a library of basic data structures. Rather than creating
overly-large arrays or reallocating memory and copying as you need more
space, it is easy to write a simple linked list. We generally do this
structure-by-structure.

Consider a structure:
```C
struct foo {
   int a;
   double b;
}
```

There are two approaches to creating a linked list of `struct foo`.
One is to define a new enclosing structure:
```C
struct foo_list {
   struct foo item;
   struct foo_list* next;
}

struct foo_list  foo_HEAD;
struct foo_list* foo_TAIL = &foo_HEAD;
```
The global (or file-static) `foo_HEAD` is a dummy entry, and we add to
the list by allocating a new `struct foo_list` (with `malloc`),
assigning it to `foo_TAIL->next`, and updating `foo_TAIL` to this new
pointer.

The other way is to combine everything into a single `struct`:
```C
struct foo {
   int a;
   double b;
   struct foo* next;
}

struct foo  foo_HEAD;
struct foo* foo_TAIL = &foo_HEAD;
```

Which you use is largely a matter of style. It is also possible to
define a generic linked list with a `void*` item, but you lose any
type information when you do this.


### Using Python

#### Writing to STDERR

Since we're expecting very specific output in STDOUT, all of your
debugging output should go to STDERR. You can do this with:
```python
print("This is an error message", file=sys.stderr)
```

You might also find the following useful:
```python
import sys

_lno = lambda: print(__file__,sys._getframe().f_back.f_lineno,file=sys.stderr)
```
Put this after your other imports. This grabs the current stack
frame, and extracts the current line number. The `__file__` variable
is replaced with the current file name. To use this, call `_lno()`
anywhere in your code, so that you can trace the program flow. This
can be extremely helpful when you're trying to track down where an
error is occurring.

#### Working with Binary Data

Binary data in python is a little more cumbersome than in C, but it is
otherwise generally easier to write code in, and has useful structures
like `list` and `dict`.

Section 7.3 of *A General Systems Handbook* briefly discusses files, but
this is from the perspective of text files. For binary files, you need
to add the specifier `b` when opening the file:

```python
with open('input_file.bin', 'rb') as in_file:
    data = in_file.read(nbytes)       # data is of type "bytes"
    hexdata = data.hex()              # hexdata is a string of hex digits
```
If you want to convert a string of hexadecimal digits back to bytes, you
can do this with:
```python
    bindata = bytes.fromhex(hexdata)  # bindata should be the same as data
```

Writing is similar:
```python
with open('output_file.bin', 'wb') as out_file:
    out_file.write(bindata)   # the bytes object above
```

Python also makes it easy to tell if an item is in a list:
```python
l = ['a' , 'b', 'c']
'a' in l # evaluates to True
'd' in l # evaluates to False
```
This also works for dictionary keys:
```python
d = dict()
d['a'] = 1
d['b'] = 2
d['c'] = 3
'a' in d # evaluates to True
'd' in d # evaluates to False
```

