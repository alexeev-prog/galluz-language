# galluz-language
<a id="readme-top"></a>

<div align="center">
  <img src="https://raw.githubusercontent.com/alexeev-prog/galluz-language/refs/heads/main/docs/logo.jpg" width="250" alt="Galluz Logo">

  <h3>A high-performance systems language combining C++ power with LLVM19 optimization</h3>

  <div>
    <a href="https://marketplace.visualstudio.com/items?itemName=alexeevdev.morning-language-syntax">
      <img src="https://img.shields.io/badge/VSCode-extension?style=for-the-badge&logo=gitbook" alt="Docs">
    </a>
    <a href="https://github.com/alexeev-prog/galluz-language/blob/main/LICENSE">
      <img src="https://img.shields.io/badge/License-GPL_v3-blue?style=for-the-badge&logo=gnu" alt="License">
    </a>
    <a href="https://github.com/alexeev-prog/galluz-language/stargazers">
      <img src="https://img.shields.io/github/stars/alexeev-prog/galluz-language?style=for-the-badge&logo=github" alt="Stars">
    </a>
  </div>
</div>

<br>

<div align="center">
  <img src="https://img.shields.io/github/languages/top/alexeev-prog/galluz-language?style=for-the-badge" alt="Top Language">
  <img src="https://img.shields.io/github/languages/count/alexeev-prog/galluz-language?style=for-the-badge" alt="Language Count">
  <img src="https://img.shields.io/github/license/alexeev-prog/galluz-language?style=for-the-badge" alt="License">
  <img src="https://img.shields.io/github/issues/alexeev-prog/galluz-language?style=for-the-badge&color=critical" alt="Issues">
  <img src="https://img.shields.io/github/last-commit/alexeev-prog/galluz-language?style=for-the-badge" alt="Last Commit">
  <img src="https://img.shields.io/github/contributors/alexeev-prog/galluz-language?style=for-the-badge" alt="Contributors">
</div>

<div align="center" style="margin: 15px 0">
  <img src="https://github.com/alexeev-prog/galluz-language/actions/workflows/static.yml/badge.svg" alt="Static Analysis">
  <img src="https://github.com/alexeev-prog/galluz-language/actions/workflows/ci.yml/badge.svg" alt="CI Build">
</div>

<div align="center">
  <img src="https://raw.githubusercontent.com/alexeev-prog/galluz-language/refs/heads/main/docs/pallet-0.png" width="600" alt="Color Palette">
</div>

Simple programming language based on S-expressions and written in LLVM &amp; C++

## Examples

### Simple

```galluz
(var (age !int) 25)
(var (price !double) 99.99)
(var (name !str) "Alice")
(var (active !bool) true)

(fprint "Name: %s, Age: %d\n" name age)
(fprint "Price: %f, Active: %d\n" price active)

(var (VERSION !double) 1.12)
(fprint "Program Version: %f\n\n" VERSION)
```

### Arithmetic

```galluz
(var (a !int) 10)
(var (b !int) 3)

(fprint "a + b = %d\n" (+ a b))
(fprint "a - b = %d\n" (- a b))
(fprint "a * b = %d\n" (* a b))
(fprint "a / b = %d\n" (/ a b))
(fprint "a %% b = %d\n" (% a b))

(var (c !double) 7.5)
(fprint "c + 2.5 = %f\n" (+ c 2.5))
(fprint "c * 2.0 = %f\n" (* c 2.0))

(defn (add !int) ((x !int) (y !int))
    (+ x y))

(defn (multiply !int) ((a !double) (b !double))
    (* a b))

(var (result1 !int) (add 5 10))
(var (result2 !double) (multiply 3.5 2.0))

(fprint "add(5, 10) = %d\n" result1)
(fprint "multiply(3.5, 2.0) = %f\n" result2)
```

### Comparison

```galluz
(var (A !int) 10)
(var (B !int) 20)
(fprint "A > B: %d\n" (> A B))
(fprint "A < B: %d\n" (< A B))
(fprint "A == 10: %d\n" (== A 10))
(fprint "A != B: %d\n" (!= A B))

(var (X !double) 5.5)
(var (Y !double) 2.2)
(fprint "X > Y: %d\n" (> X Y))
(fprint "X < Y: %d\n" (< X Y))
(fprint "X == 5.5: %d\n" (== X 5.5))
```

### Global and local vars

```galluz
(global (ALPHA !int) 42)
(scope
    (var (BETA !str) "Hello")
    (fprint "BETA: %s\n" BETA)
)
(fprint "ALPHA: %d\n" ALPHA)
```

### If-else

```galluz
(var (age !int) 18)

(if (>= age 18)
    (fprint "Adult\n")
    (fprint "Minor\n"))
```

### While loop

```galluz
(var (counter !int) 0)

(while (< counter 5)
    (scope
        (fprint "Counter: %d\n" counter)
        (set counter (+ counter 1))
    )
)
```

### Vars change

```galluz
(var (X !int) 10)
(fprint "X before: %d\n" X)
(set (X !int) 20)
(fprint "X after: %d\n" X)
```

### Factorial

```galluz
(defn (iter_factorial !int) ((n !int))
    (if (< n 0)
        (do
            (fprint "Error: Negative number\n")
            -1)
        (do
            (var (result !int) 1)
            (var (i !int) 1)
            (while (<= i n)
                (do
                    (set result (* result i))
                    (set i (+ i 1))))
            result)))

(defn (factorial !int) ((x !int))
    (if (< x 0)
        -1
        (if (== x 0)
            1
            (* x (factorial (- x 1)))
        )
    )
)

(fprint "iter_factorial(5) = %d\n" (factorial 5))
(fprint "iter_factorial(-3) = %d\n" (factorial -3))
(fprint "iter_factorial(0) = %d\n" (factorial 0))
(fprint "iter_factorial(1) = %d\n" (factorial 1))

(fprint "factorial(5) = %d\n" (factorial 5))
(fprint "factorial(-3) = %d\n" (factorial -3))
(fprint "factorial(0) = %d\n" (factorial 0))
(fprint "factorial(1) = %d\n" (factorial 1))
```

### Structs

```galluz
(struct User ((name !str) (age !int)))

(defn (is_adult_user !void) ((user !User))
    (if (>= (getprop user age) 18)
        (fprint "Adult\n")
        (fprint "Minor\n")
    )
)

(defn (set_age !void) ((user !User) (new_age !int))
    (setprop user age new_age)
)

(var (user !User) (new User (name "John") (age 18)))
(is_adult_user user)
(set_age user 17)
(is_adult_user user)
(fprint "%d\n" (hasprop user age))
```

### finput

```galluz
(var (name !str) (finput "Enter your name: "))
(fprint "Hello, %s!\n" name)

(var (age !int))
(var (height !double))
(finput "Enter age and height (example: 25 1.75): " age height)
(fprint "Age: %d, Height: %f\n" age height)

(var (new_age !int) (finput "How old are you? " !int))
(var (new_height !double) (finput "What is your height? " !double))
(fprint "You are %d years old and your height is %f meters\n" new_age new_height)

(var (a !int))
(var (b !double))
(var (c !int))
(finput "Enter int, double, int: " a b c)
(fprint "a=%d, b=%f, c=%d\n" a b c)

(var (city !str) (finput "Enter your city: " !str))
(fprint "You live in %s\n" city)

(var (is_student !bool))
(finput "Are you a student? (1=yes, 0=no): " is_student)
(if is_student
    (fprint "You are a student\n")
    (fprint "You are not a student\n")
)
```

### Modules

```galluz
(import "core/basicmath.glz" (module AddMath) (module MultiplyMath))

(moduleuse AddMath)

(fprint "%d\n" (add 5 10))
(fprint "%d\n" (MultiplyMath.multiply 5 10))
```

File `core/basicmath.glz`:

```glz
(defmodule AddMath
    (defn (add !int) ((x !int) (y !int))
        (+ x y))
)

(defmodule MultiplyMath
    (defn (multiply !int) ((a !int) (b !int))
        (* a b))
)
```

## Galluz Manifesto
*"We reject the false choice between performance and expressiveness.
We reject the old methods imposed by backward compatibility with
long-dead legacy products. Galluz is a new era in programming
that combines the simplicity of S-expressions with the functionality
of C++. Thanks to the purity of the project and its versatility,
you can create anything and everything you want."*

<div align="center">
  <br>
  <a href="#readme-top">↑ Back to Top ↑</a>
  <br>
  <sub>Made with LLVM 19</sub>
</div>

