
# Verily: A proof system

Jordan Dehmel, 2025-present

## About

This began as an SMT implementation I wrote for Computer-Aided
Verification at CU Boulder in Fall '25. I then translated it
from Python to C++ and continued expanding it. The deductive
proof system (mostly separate from the SMT stuff) came in Spring
'26 when I was taking a logic course.

**This is not necessarily decidable!** If you choose your rules
wisely, it might be, but it almost certainly will not be if you
aren't smart about it. A poor choice of rules can make proving
"2 is a natural number" take seconds. This is NP-complete for
obvious reasons, so tread with care!

## This Repo

This repo contains some examples, a verily CLI, some tests, and
a verily syntax highlighting extension for `vscode`.

To build the CLI, just run `make` from the root of this repo.
This will create `./verily.out`, which is the executable. Run
`./verily.out --help` for help. View the examples to see
verily's syntax.

To install the `vscode` extension, change directories to
`verily-highlighting`. Then, run `npx @vscode/vsce package` to
build the extension. This will produce a local file ending in
`.vsix`. If this file were named `NAME_GOES_HERE.vsix`, you
would then run `code --install-extension NAME_GOES_HERE.vsix`.
You must then reload your `vscode` window for the syntax
highlighter to start working.

## Example

Given the file:

```verily
rule typed_instantiation:
  over Domain, consequent, x, y
  given
    forall x. x in Domain implies consequent,
    y in Domain
  deduce consequent[x = y]
;

rule modus_ponens:
  over p, q
  given p, p implies q
  deduce q
;

axiom: 0 in Nat;
axiom: forall n. n in Nat implies S(n) in Nat;

// We could use untyped instantiation, but it would be much less
// efficient
axiom: even(0);
axiom: forall n. n in Nat implies
  (even(n) implies not even(S(n)));
axiom: forall n. n in Nat implies
  (not even(n) implies even(S(n)));

theorem: even(S(S(0)));
```

We could run the theorem prover:

```sh
./verily.out FILENAME_GOES_HERE.verily
```

And receive a proof:

```
(theorem (e (S (S 0))) (rule_application (rule (over p q) (given
p (implies p q)) q) (premises (theorem (not (e (S 0)))
(rule_application (rule (over p q) (given p (implies p q)) q)
(premises (axiom (e 0)) (theorem (implies (e 0) (not (e (S 0))))
(rule_application (rule (over Domain consequent x y) (given
(forall x (implies (in x Domain) consequent)) (in y Domain))
(REPLACE consequent x y)) (premises (axiom (forall n (implies
(in n Nat) (implies (e n) (not (e (S n))))))) (axiom (in 0 Nat))
)))))) (theorem (implies (not (e (S 0))) (e (S (S 0))))
(rule_application (rule (over Domain consequent x y) (given
(forall x (implies (in x Domain) consequent)) (in y Domain))
(REPLACE consequent x y)) (premises (axiom (forall n (implies
(in n Nat) (implies (not (e n)) (e (S n)))))) (theorem (in (S 0)
Nat) (rule_application (rule (over Domain consequent x y) (given
(forall x (implies (in x Domain) consequent)) (in y Domain))
(REPLACE consequent x y)) (premises (axiom (forall n (implies
(in n Nat) (in (S n) Nat)))) (axiom (in 0 Nat)))))))))))
```

This proof shows the series of rule applications by which you
can derive the theorem from axioms. If no proof can be found,
it will report an error (this does *not* mean that the
to-be-theorem is false, nor does it mean that it is unprovable
within the system).

## Operators and Quantifiers

The following are operators which will be parsed. Note that,
since the entire language is uninterpreted, straying from these
will not cause any errors: It just might not parse the way you
expect.

Operators (in precedent order):
- `in`
- `==`
- `not`
- `and`
- `or`
- `implies`
- `iff`

Parentheses and `python`-syntax function calls are also
built-in. Symbols do not need to be defined before use.

```verily
axiom: forall x. f(x) == foo;
```

For instance, `a in b == c in d and not e or f implies g iff h`
would parse equivalently to
`(((((a in b) == (c in d)) and (not e)) or f) implies g) iff h`.

Since `a and b or c` is ambiguous in most languages, it is
considered bad form to write it: Instead, write either
`(a and b) or c` or `a and (b or c)`.

## Rules

ASTs are well-formed formulae over the language. They can be
asserted as axioms via `axiom` statements. Theorems are derived
from axiomatic ASTs via inference rules.

An inference rule takes the form

```verily
rule:
  over x, y, z
  given fe, fi, fo
  deduce fum
;
```

Inference rules are operations on uninterpreted ASTs. **Given**
that some AST is a theorem, we can **deduce** that some other
AST is as well. The **over** clause lists free variables. Note
that, in verily, we start from the AST to be proven and work
backwards to axioms: If a proof exists within the system, it
will be found by this procedure (G{\"o}del's completeness
theorem). The **over** clause of a rule declares the free
variables. Because of the way the deduction system works, all
free variables must occur in the "deduce" section of the rule.

An inference rule "rule: over x given A, B deduce C" can be read
"for all $x$, if $A$ and $B$ are theorems, so is $C$".

The "over" and "given" sections of a rule are optional, but the
"deduce" section is required.

## Rule Examples

This is an incomplete list of encodings of basic principles.
These are **not** built-in: No inference rules are.

```verily
# Boolean logic
rule:
  over x, y
  given x, y
  deduce x and y
;
rule:
  over x, y
  given x
  deduce x and y
;

# Modus ponens
rule:
  over p, q
  given p, p implies q
  deduce q
;

# Modus tollens
rule:
  over p, q
  given not q, p implies q
  deduce not p
;

# Hypothetical syllogism / transitivity of implication
rule:
  over a, b, c
  given a implies b, b implies c
  deduce a implies c
;

# Disjunctive syllogism
rule:
  over p, q
  given p or q, not p
  deduce q
;

# Instantiation
rule:
  over f, y
  given forall x. f(x)
  deduce f(y)
;

# Generalization
rule:
  over f, y
  given f(y)
  deduce exists x. f(x)
;

# Danger zone!
rule:
  over a
  deduce a or not a
;

# Double negation elimination
rule:
  over a
  given not not a
  deduce a
;
```

Inference rules without "given" clauses are axioms or axiom
schemas.

## Axioms and Theorems

Axioms are things that are assumed to be true. They are declared
using the `axiom` keyword.

```verily
axiom: is_even(0);
```

Theorems are *not* assumed to be true: They must be proven via
the known induction rules and axioms. They use the same syntax,
but use the `theorem` keyword.

```
theorem: is_even(S(S(0)));
```

Theorems are interchangeably referred to as "annotations"
throughout. If they cannot be proven, an error will be raised.
Once they are proven, they act the same as axioms: Another
proof can use them without re-proving them.

## Functions and Methods

Functions are purely functional (possibly recursive), while
methods are imperative. They are two syntaxes which can express
equivalent computational tasks. Functions can be analyzed
directly by the system, but methods must be analyzed via Hoare
logic.

## Backward Deduction

A rule is defined as follows. Given free variables
$f_1, \ldots, f_n$, antecedents $A = \{a_1, \ldots, a_m\}$, and
consequence $C$:

$$
  \forall f_1. \ldots. \forall f_n.
  \left(\bigwedge_{a \in A} a(f_1, \ldots, f_n)\right)
  \implies C(f_1, \ldots, f_n)
$$

We want to work backwards: Given some theorem to prove, find the
theorems which prove it, and the theorems to prove those, etc.
This is not necessarily decidable or more efficient than forward
search, but in practice usually is. We do this by pattern
matching on the consequence of generic deduction rules.
Deduction rules have some number of free variables, some number
of antecedents / requirements, and exactly one consequence. They
are meta-statements that say "for any instances of the free
values under which all the antecedents are theorems, we can
deduce that the consequence (under those same values) is too".

For instance, the $\land$ rule "$P$ and $Q$ implies $P \land Q$"
would have the free variables $\{P, Q\}$, the antecedents
${P, Q}$, and the consequence $P \land Q$.

**Forward search:** If we knew the statements
$\texttt{isRaining}$ and $\lnot \texttt{isSunny}$ to be
theorems already, we could apply this rule to deduce
$\texttt{isRaining} \land \lnot \texttt{isSunny}$.

**Backward search:** We want to show
$\texttt{isRaining} \land \lnot \texttt{isSunny}$. We can use
pattern matching on the consequence of our rule to obtain
$P := \texttt{isRaining}$ and $Q := \lnot \texttt{isSunny}$.
Therefore, it would be sufficient to prove that these are
theorems.

### Alternation

Consider Modus Ponens: "$P$ and $P \implies Q$ implies $Q$".
Suppose we want to show "$\lnot \texttt{isSunny}$". Further
suppose that
$\texttt{isRaining} \implies \lnot \texttt{isSunny}$ and
$\texttt{isRaining}$ are both *derivable* theorems (EG applying
backward deduction to them would prove them), but that neither
are known to the prover.

Forward search would, starting from axioms, eventually discover
the above antecedents and apply MP. However, a backward system
could not! There is no way to derive the free variable $P$!

Formally:

$$
  \forall P. \forall Q. (P \land (P \implies Q)) \implies Q
$$

Under any backward deduction, the best we can get is:

$$
  \forall P. (P \land (P \implies Q)) \implies Q
$$

Now the question of proving $Q$'s theoremhood is equivalent to
finding $P$ such that both $P$ and $P \implies Q$ are theorems:
There are countably infinitely many possible values for $P$, so
this is undecidable (consider the case where no such $P$
exists).

The core issue here is that there is a free variable which does
not occur in the consequence. There is a dual issue for forward
provers when a free variable does not appear in the antecedents.

 FVs in antecedents | FVs in consequence | Notes
--------------------|--------------------|----------------------
 Not all            | Not all            | Neither method works
 Not all            | All                | Backward only
 All                | Not all            | Forward only
 All                | All                | Both methods work

The solver's solution here is **alternation**: When a `theorem`
statement is found, it will try backwards deduction until that
can go no further. Then, it will try forward deduction until
that can go no further. This will continue until the theorem is
proven or the number of allotted deduction passes is exhausted.
