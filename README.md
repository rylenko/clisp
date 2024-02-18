<h1 align="center">Welcome to CLISP 🌿</h1>

Simple lisp like interpreter with standard library, storage of variables in the form of a hash table, etc.

<h1 align="center">Make</h1>

```
$ make
```

To install:

```
$ sudo make install
```

To uninstall:

```
$ sudo make uninstall
```

To clean:

```
$ make clean
```

<h1 align="center">Usage</h1>

Run interpreter with standard library:

```
$ clisp
```

Run without standard library:

```
$ clisp --no-std
```

Execute files:

```
$ clisp program.clisp
```

Simple examples:

```
>>> = {name} (input "Name: " 20)
Name: qwerty
()

>>> name
"qwerty"

>>> if (== name "John Doe")
	{print "anonymous user"}
	{print (join "Hello, " name "!")}
"Hello, qwerty!"
()
```

```
>>> day_name 0
"Monday"
```

```
>>> fun {square x} {* x x}
()
>>> = {numbers} {1 2 3 4 5}
()
>>> = {numbers} (map numbers square)
()
>>> numbers
{1.000000 4.000000 9.000000 16.000000 25.000000}
```

```
>>> sum {1 2 3 4 5}
15.000000

>>> drop {1 2 3 4 5} 2
{3.000000 4.000000 5.000000}

>>> in {1 2 3} 2
1.000000
>>> in {1 2 3} 5
0.000000

>>> len {1 2 3}
3.000000

>>> head {1 2 3}
{1.000000}
>>> tail {1 2 3}
{2.000000 3.000000}

>>> fib 10
55.000000
```

```
>>> fun {number_to_string x} {
	case x
    	{1 "one"}
    	{2 "two"}
    	{3 "three"}
    	; unimplemented
}
()

>>> number_to_string 2
"two"
>>> number_to_string 4
Error: No case found.
```

```
>>> fun {number_to_string x} {
	select
    	{(== x 1) "one"}
    	{(== x 2) "two"}
    	{(== x 3) "three"}
    	{otherwise "unimplemented"}
}
()

>>> number_to_string 2
"two"
>>> number_to_string 4
"unimplemented"
```

<h1 align="center">Todo</h1>

- [Garbage collector](http://journal.stuffwithstuff.com/2013/12/08/babys-first-garbage-collector/)?

- Man page.
