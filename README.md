
# Verily: A toy inductive proof system

Jordan Dehmel, 2025-2026

## About

**This is not necessarily decidable!** If you choose your rules
wisely, it might be, but it almost certainly will not be if you
aren't smart about it. A poor choice of rules can make proving
"2 is a natural number" take seconds. Tread with care!

You can do a few things.

1. Add things known to be true via `axiom`s
2. Introduce ways of deriving new truths from known truths via
  `rule`s
3. Query whether we can deduce something to be true via
  `theorem`s
4. Manually derive new truths from existing ones via `apply`

**Everything is uninterpreted!** Though operators like `+`, `*`,
etc are built-in and have the expected precedence,
**they don't do anything!** They are just ASTs!

## This Repo

This repo contains some examples, a verily CLI, some tests, and
a verily syntax highlighting extension for `vscode`.

To build the CLI, just run `make` from the root of this repo.
This will create `./verily.out`, which is the executable. Run
`./verily.out --help` for help. View the examples to see
verily's syntax.

To install the `vscode` syntax highlighting extension, change
directories to `verily-highlighting`. Then, run
`npx @vscode/vsce package` to build the extension. This will
produce a local file ending in `.vsix`. If this file were named
`NAME_GOES_HERE.vsix`, you would then run
`code --install-extension NAME_GOES_HERE.vsix`. You must then
reload your `vscode` window (ctrl+shift+p
'Developer: Reload Window') for the extension to start
syntax-highlighting any open files with the extension `.verily`.

## Example

Given the file:

```verily
rule typed_instantiation:
  over Domain, consequent, x, y
  given
    forall x in Domain. consequent,
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
- `'` ("prime", suffix unary)
- `^`
- `*`
- `/`
- `%`
- `+`
- `-`
- `<`
- `>`
- `<=`
- `>=`
- `==`
- `cross`
- `to`
- `in`
- `not` (prefix unary)
- `or`
- `and`
- `iff`
- `implies`
- `derives`
- `models`

Unless otherwise specified, the above operators are infix binary
(for operator `x` and terms `A`, `B`, it would look like `A x B`
and parse to S-EXPR `(x A A)`).

Parentheses and `python`-syntax function calls (and tuples) are
also built-in. Symbols do not need to be defined before use. All
statements should end in semicolons.

Since `a and b or c` is ambiguous in most languages, it is
considered bad form to write it: Instead, write either
`(a and b) or c` or `a and (b or c)`, depending on what you
actually want.

Any single token can be used as a quantifier. Technically, the
parser recognizes any expression of the form
`TOKEN EXPR . EXPR` as a quantified expression. The basic ones
are `forall`, `exists`, and `lambda`, although
**none of these are interpreted**. If you want them to perform
their intuitive functions, you need to add appropriate rules.

Calls are written `f(x)`. Only unary calls are built-in. If you
wanted to provide multiple arguments (interpretation-specific),
you could say `f(x)(y)` (Currying) or `f((x, y))` (providing a
single 2-tuple).

### Meta-implication

The deduction theorem goes something like this: "If we can
derive $B$ by assuming $A$, then $A \implies B$". This is a
meta-theorem, since it deals with assumptions. Since a user may
or may not want to allow this rule in their proofs, verily can
take it or leave it. If you enable the meta-prover via the
`--meta_prove` CLI flag or the `setting "meta_prove=true";`
verily statement, this will be a valid rule.

## Importing other files

You can use the `include "local_filepath";` command. This takes
a path relative to the current file (the CWD in CLI mode).

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

## Rules

Theorems are derived from axioms via inference rules.
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

## Keywords

Despite repeated claims to the contrary, there are actually a
few words which mean something specific in verily. Each of them
starts a statement. The following are those keywords, with their
form and purpose.

`rule` introduces a new rule. See previous sections for the
form of these statements.

`axiom: EXPR;` introduces an axiom (statement which can be
taken to be true without proof).

`theorem: EXPR;` tells the solver to try to find a proof for
the given expression before moving on to the next statement.

`prove_forward: EXPR;` works the same as `theorem`, but starts
in forward derivation mode instead of the default backwards.

`setting "SOME_STR";` depending on the string, sets some
internal setting.

`include "SOME_STR";` runs the given (local-path, like EG `C++`)
file, executing each statement in that file before moving onto
the next statement in the current file.

`function NAME(untyped_arg, b: TypeOfB, c in TypeOfC) REQS_AND_ENS { EXPR }`
is a shorthand statement which introduces a rule describing the
given function (EG if all the requirements are met, then a call
to the function is `==` to its definition). `REQS_AND_ENS` is
any number of `requires EXPR` and/or `ensures EXPR` clauses.
There is no punctuation terminating each expression in these
clauses or in the body (EG no semicolons).

`method NAME(ARGS) returns VAR REQS_AND_ENS { METHOD_STATEMENTS }`
is an unimplemented statement. It will parse, but will not do
anything else. Since it is unimplemented, we will not give the
grammar of `METHOD_STATEMENTS` here (it's just a basic
imperative sublanguage with `let`, `while`, `if`, etc).

### Synonymous keywords

There are many synonymous keywords. Some of them are listed
below.

Synonymous with `function`:
- `fn` (ease of use)

Synonymous with `include`:
- `import` (ease of use)

Synonymous with `setting`:
- `option` (ease of use)

Synonymous with `theorem`:
- `lemma` (for organization)
- `deduce` (because it's used for that within rules)
- `prove` (for compatibility)
- `prove_backward` (to match the form of `prove_forward`)
- `assert` (for compatibility)

Synonymous with `axiom`:
- `assume` (for compatibility)

## Functions and Methods

Functions are purely functional (possibly recursive), while
methods are imperative. They are two syntaxes which can express
equivalent computational tasks. Functions can be analyzed
directly by the system, but methods must be analyzed via Hoare
logic (currently unimplemented). Functions and methods can be
annotated with the `requires` and `ensures` keywords, each of
which can be followed by a single expression.

```verily
function fib_1(n: Nat) {
  if_then_else(
    n < 2,
    n,
    fib_1(n - 1) + fib_1(n - 2)
  )
}

method fib_2(n: Nat) returns ret {
  if n < 2 {
    ret = n;
  } else {
    ret = fib_2(n - 1) + fib_2(n - 2);
  }
}
```

As of writing, methods are not implemented: They will parse, but
they won't do anything else. Functions are supported, and their
signatures and bodies will be declared equivalent (EG `==`) via
an implicitly-defined rule.

## Manual Forward Deduction

We can manually add pending theorems via `wts` (Want To Show).
We can manually apply rules via the `apply` keyword.

```verily
wts S(S(S(0))) in Nat;
  apply application_typing to succ, zero_nat as one_nat;
  apply application_typing to succ, one_nat as two_nat;
  apply application_typing to succ, two_nat as three_nat;
```

All of the following are valid formulations.

- `apply X;` (applies rule `X` to every viable existing case)
- `apply X to Y;`
- `apply X to Y, Z, A;`
- `apply X to A as Y;`
- `apply all;` (applies all rules to every viable existing case
  exactly once)

## Backward Deduction

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

## Lemmas, Efficiency, and a Silly Analogy

Think of the known truths as one point $A$ in space and the
desired theorem as another point $B$. If we use forward
derivation, think of circles emanating from $A$: In order for
$B$ to be proven, the circles must reach it.

In alternating mode, circles are emitted from both $A$ and $B$,
and they only have to touch in order to prove the theorem. The
expected cumulative area (number of potential theorems examined)
of the circles is much smaller!

Now imagine we put a third point $C$ directly in between $A$ and
$B$: This is a well-designed lemma. Now we have circles going
out from all three points, and we only need $A$ and $C$ to touch
and $C$ and $B$ to touch. The expected cumulative area of the
circles is even smaller!

Note that the solver could give up due to resource constraints
without a lemma but quickly find a proof with a lemma. However,
a bad lemma could lie completely off the path between the known
truth and the would-be theorem, in which case it will only waste
solver time. In general, designing good lemmas is about as hard
as finding a proof. However, if you already have a proof in
mind, a good lemma can help the solver connect the dots.

Technically speaking, backwards proof search is more like bolts
of lightning shooting off the point in random directions, hoping
to touch the known truth. This is because forwards proof search
is breadth-first while backwards proof search is depth-first.
