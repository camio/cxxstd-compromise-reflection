---
title: "User-friendly and Evolution-friendly Reflection: A Compromise"
document: P1733R0
date: 2019-06-15
audience: SG7 Reflection
author:
  - name: David Sankel
    email: <dsankel@bloomberg.net>
  - name: Daveed Vandevoorde
    email: <daveed@edg.com>
toc: true
toc-depth: 4
---

# Abstract

The two primary `constexpr`-based reflection proposals for C++ are at odds with
eachother. "constexpr reflexpr" (@P0953R2) focuses on a simple and familiar
API, but suffers from from potential performance issues and could potentially
make evolution of the language difficult. "Scalable Reflection in C++"
(@P1240R0) solves these two issues, but it comes with a significant cost in
terms of usability. This paper is an attempt to bridge the two approaches by
introducing the generally useful feature, parameter constraints, and providing
guidelines for a reflection API that is both user-friendly and has a
straightforward evolutionary path as the language changes.

# Introduction

## Brief History of Reflection in C++

## Typeful Reflection and its Drawbacks

## Monotype Reflection and its Drawbacks

# Parameter Constraints

Parameter constraints are an extension of concepts such that parameters can be
utilized in `requires` clauses. Consider the following overload set for the
familiar power function:

```c++
double pow( double base, int iexp );
double pow( double base, int iexp ) requires (iexp == 2); // proposed
```

The second declaration is identical to the first except it has an additional
constraint that the second argument is `2`. Its implementation could be heavly
optimized given this additional information.

Overload resolution in C++ happens at compile time, *not* run time, so how
could this ever work? Consider the call to `pow` in the following function.

```c++
void f(double in) {
    in += 5.0;
    double d = pow(in, 2);
    // ...
}
```

Here the compiler knows *at compile time* that the second argument to `pow` is
`2` so it can theoretically make use of the overload with the parameter
constraint. In what other cases does the compiler know at compile time the
value of a parameter?

As it turns out, we already have standardese for such an argument (or generally
an expression) in C++: *constant expression*.

In short, this concepts extension will allow for parameter identifiers to
appear in requires clauses and during overload resolution:

- if the argument is a *constant expression* it is evaluated as part of
  evaluation of the requires clause, and
- if the argument is *not* a *constant expression* the entire overload is
  discarded.

## Relation to clang's `enable_if` `__attribute__`

As it turns out there already exists similar functionality implemented as an
experimental clang extension (See @ClangAttributes). The above example can be
written as follows using this extension:

```c++
double pow( double base, int iexp );
double pow( double base, int iexp ) __attribute__((enable_if(iexp == 2, "")));
```

The `enable_if` clang attribute, when combined with its `unavailable`
attribute, can be used to effectively check some precondition violations at
compile time. See the following example taken from clang's documentation:

```c++
int isdigit(int c);
int isdigit(int c)
  __attribute__((enable_if(c <= -1 || c > 255, "chosen when 'c' is out of range")))
  __attribute__((unavailable("'c' must have the value of an unsigned char or EOF")));

void foo(char c) {
  isdigit(c);
  isdigit(10);
  isdigit(-10);  // results in a compile-time error.
}
```

Our proposed concepts extension can solve this problem as well by making use of
`static_assert` with a template. 

```c++
int isdigit(int c);

template<bool always_false=false>
int isdigit(int c) requires(c <= -1 || c > 255)
{
    static_assert(b, "'c' must have the value of an unsigned char or EOF");
}
```

# User-friendly and Evolution-friendly Reflection

Parameter constraints are a neat feature, but what do they have to do with
reflection?

@P0953R2's user-centric API calls for a type hierarchy representing various
elements of C++'s abstract syntax tree. This tree could change significantly
over time with new revisions of the language. Because of this, `reflexpr`
expressions should not result in values whose type are tightly bound to this
hierarchy. Instead, these these values should be *convertable* to values within
the hierarchy.

```c++
constexpr
meta::cpp20::type t = reflexpr(int); // reflexpr(int) produces a meta::info
                                     // object which is converted to a
                                     // meta::cpp20::type object.
```

Conversions to the `meta::cpp20` hierarchy can be made cleanly and without
templates using parameter constraints in conversion constructors.

```c++
namespace meta::cpp20 {
class type {
  public:
    consteval type(meta::info i) requires(meta::is_type(i));
    //...
};
class class_ {
  public:
    consteval class_(meta::info i) requires(meta::is_class(i));
    //...
};
}
```

However, to provide users with seamless interaction with overloading, the following needs to be
supported somehow.

```c++
void print(meta::cpp20::namespace t); // #1
void print(meta::cpp20::type v);      // #2
void print(meta::cpp20::class_ c);    // #3
//...

namespace foo { /*...*/ }
class bar{};

void f() {
  print(reflexpr(foo)); // Matches #1
  print(reflexpr(int)); // Matches #2
  print(reflexpr(bar)); // Desire to match #3, but ambiguous between #2 and #3.
}
```

This can be solved, however, by making conversions from `meta::info` objects
only to the bottom-most-leaves (or logically "most derived" classes) of the
type hierarchy.

```c++
namespace meta::cpp20 {
class type {
  public:
    consteval type(meta::info i) requires( meta::is_type(i)
                                       && !meta::is_class(i)
                                       && !meta::is_union(i)
                                       && !meta::is_enum(i) );
    //...
};
class class_ {
  public:
    consteval class_(meta::info i) requires( meta::is_class(i) );
    //...
};
}
```

Now `print(reflexpr(bar))` will unambiguously select the desired overload.

## Upcasting and Downcasting

Once in the type hierarchy, casting upward can be implemented in the usual way.

```c++
namespace meta::cpp20 {
class class_ {
  public:
    consteval class_(meta::info i) requires( meta::is_class(i) );
    
    consteval operator type();
    //...
};
}
```

```c++
class bar{};
void g() {
    constexpr meta::cpp20::class_ c = reflexpr(bar);
    meta::cpp20::type t = c;
}
```

A downcast function template can be provided to go in the reverse direction.

```c++
namespace meta::cpp20 {
    consteval class_ make_class_from_info(meta::info i);
        // Fails with an exception if !meta::is_class(i)

    template<typename T>
    consteval T downcast(meta::cpp20::type t) {
        if constexpr (std::is_same_v<T, meta::cpp20::class_ ) {
            return make_class_from_info(t.info());
        } else //...
    }
}
```

```c++
class bar{};
void h() {
    constexpr meta::cpp20::type t = reflexpr(bar);
    constexpr auto c = meta::cpp20::downcast<meta::cpp20::class_>(t); // OK
}
```

For programmer convenience, we can additional provide a `most_derived` function
which will take in a `meta::cpp20::object` (the most base class in the
hierarchy) and return an instance of the most derived type for that object.

```c++
namespace meta::cpp20 {
    consteval std::vector<type_> get_member_types(class_ c) const;
}

struct baz {
    enum E { /*...*/ };
    class Buz{ /*...*/ };
    using Biz = int;
};

void print(meta::cpp20::enum_);   #1
void print(meta::cpp20::class_); #2
void print(meta::cpp20::type);   #3

void f() {
    constexpr meta::cpp20::class_ metaBaz = reflexpr(baz);
    for...(constexpr meta::cpp20::type member_ : get_member_types(metaBaz)) {
        print( meta::cpp20::most_derived(member_) ); // Calls #1, #2, and then #3
    }
}
```

`most_derived` can be implemented using parameter constraints:

```c++
namespace meta::cpp20 {

template<bool dummy=true>
consteval type most_derived(object o) requires(  meta::is_type(o.info())
                                             && !meta::is_class(o.info())
                                             && !meta::is_union(o.info())
                                             && !meta::is_enum(o.info()))

template<bool dummy=true>
consteval enum_ most_derived(object o) requires( meta::is_enum(o.info()))

//...
}
```

## Evolution

TODO: discuss the use of namespaces (`meta::cpp20`, `meta::cpp23`), how
language design would impact those changes, and how backwards compatibility
would work.

# Conclusion

---
references:
  - id: P0953R2
    citation-label: P0953R2
    title: "constexpr reflexpr"
    author:
      - family: Chochlík
        given: Matúš
      - family: Naumann
        given: Axel
      - family: Sankel
        given: David
    issued:
      year: 2019
    URL: http://wg21.link/P0953R2
  - id: N4766
    citation-label: N4766
    title: "Working Draft, C++ Extensions for Reflection"
    author:
      - family: Chochlík
        given: Matúš
      - family: Naumann
        given: Axel
      - family: Sankel
        given: David
    issued:
      year: 2018
    URL: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/n4766.pdf
  - id: P1240R0
    citation-label: P1240R0
    title: "Scalable Reflection in C++"
    author:
      - family: Sutton
        given: Andrew
      - family: Vali
        given: Faisal
      - family: Vandevoorde
        given: Daveed
    issued:
      year: 2018
    URL: http://wg21.link/P1240R0
  - id: ClangAttributes
    citation-label: ClangAttributes
    title: "Attributes in Clang"
    issued:
      year: 2019
    URL: http://clang.llvm.org/docs/AttributeReference.html#enable-if

---
