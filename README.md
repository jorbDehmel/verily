
# Verily: A Dogmatic Theorem Prover

Jordan Dehmel, 2025-2026

## About

**This is not necessarily decidable!** If you choose your rules
wisely, it might be, but it almost certainly will not be if you
aren't smart about it. A poor choice of rules can make proving
"2 is a natural number" take seconds, or not verify at all!
Tread with care!

Verily is a theorem prover based on term rewriting rules.
It is *not* based on the Curry-Howard isomorphism. An axiom is a
syntax tree which is taken to be a theorem without proof, and a
theorem is a syntax tree which is derived from known theorems
via known rules. A tree is said to be proven if it is a theorem.
Verily finds theorems given axioms and rules, and also allows
you to apply rules yourself.

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

This is a string-rewriting-rule-based theorem prover, **not** a
Curry-Howard-isomorphism-based one!

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
# I am a comment
// I am also a comment
-- I, too, am a comment

axiom zero_in_nat: 0 in Nat;
axiom successor_fn: S in Nat to Nat;

rule fn_app_typing:
  over f, x, A, B
  given f in A to B, x in A
  deduce f(x) in B
;

# These two applications manually prove the theorem
# apply fn_app_typing
#   to successor_fn,
#   zero_in_nat
#   as one_in_nat
# ;
# apply fn_app_typing
#   to successor_fn,
#   one_in_nat
#   as two_in_nat
# ;

# Automatically prove theorem
prove_forward: S(S(0)) in Nat;
```

We could run the theorem prover:

```sh
./verily.out FILENAME_GOES_HERE.verily
```

And receive a proof:

```
(theorem
  (in (@ S (@ S 0)) Nat)
  (rule_application
    (rule fn_app_typing)
    (premises
      (axiom (in S (to Nat Nat)))
      (theorem
        (in (@ S 0) Nat)
        (rule_application
          (rule fn_app_typing)
          (premises
            (axiom (in S (to Nat Nat)))
            (axiom (in 0 Nat))
          )
        )
      )
    )
  )
)
```

This proof shows the series of rule applications by which you
can derive the theorem from axioms. If no proof can be found,
it will report an error. Be careful: Just because a statement
does not follow from your rules does not mean it is false! And
just because verily fails to find entailment does not mean that
a derivation doesn't exist!

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

### Synonymous keywords

There are many synonymous keywords. Some of them are listed
below.

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

FVs are Free Variables, EG the `over ...` clause.

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

# Future

I intend to make some major revisions to Verily, now that I'm
more familiar with modern interactive theorem provers.

Revisions:
- [X] Theorems should take rule form (EG
  `given ... deduce ... by`)
- [X] Goals and tactics
- [X] All rules should be considered axioms
- [X] No unicode, and no abbreviated keywords! Make this dogma!
- [X] Extensible parsing is needed (IE lean's `notation`
  keyword)
- [X] Move from $f(x, y)$ syntax to $f x y$ syntax (follows from
  extensible parsing)
- [X] Remove CDCL and congruence: This should be completely
  generic. No extensions! That's dogma!

The major difference is that `verily` is based on term rewriting
rules, where `lean` and `coc/coq/roq` are based on the
Curry-Howard isomorphism. Theoretically, `verily` should be
better at backward deduction. Unlike `lean`, `verily` has no
objects: Only rules. If you want to alias something, you do it
through rules.

## Examples of revised verily:

```verily
-- Axioms and rules are the same thing now
axiom LEM:
  over x
  given x : Bool
  deduce x or not x
;

axiom EqualDef:
  over phi, A, B
  given A = B
  deduce phi[a := B]
;

-- fizz reduces to buzz, buzz reduces to fizz
bidirectional axiom:
  over foo
  given fizz
  deduce buzz
;

-- Updated theorem syntax
theorem foo
  over A, B, C
  given a, b, c
  deduce conclusion_goes_here
by {
  step_1
  step_2
  step_3
}

-- Updated function call parsing
axiom:
  over f, x, A, B
  given f : A -> B, x : A
  deduce (f x) : B
;

-- A non-axiom special form of rule
let foo := value;
```

Lean's `have` would be forwards rule applications, and its
`apply` would be backwards rule applications.

```verily
theorem:
  over m, n
  given
    -- Reserved notation for naming hypotheses
    h1 := m : Nat,
    h2 := n : Nat
  deduce m + n : Nat
by {
  ls
  -- h1 := m : Nat, h2 := n : Nat |- m + n : Nat
  forward nat_add_is_nat as h3
  ls
  -- ..., h3 := m + n : Nat |- m +  n : Nat
  h3
}
```

Perhaps `apply ... to goal` (backward) vs `apply ...`
(implicitly forward)? And/or `forward ...` vs `backward ...`.
All rules have one conclusion, so we will never
split when going forward, but we may introduce goals when going
backward. The theorem will only exit without error if all goals
have been proven. We will introduce `wts` (or maybe `focus`?)
to focus on a specific goal. `lemma` will create a temporary
goal, inheriting all presuppositions, then add it as a fact
once proven.

We could have the keywords `push forward` / `push backward` /
`alternate`, though we should discourage their use in actual
proofs. Maybe by marking them with ending question marks.

```
theorem foo
  deduces conclusion
by {
  forward rule_1  -- alt "fw"
  backward rule_2 -- alt "bw"
  wts antecedent_1 {
    alternate
  }
  wts antecedent_2 {
    forward rule_3
    lemma temp_goal {
      alternate
    }
  }
}
```

`forward` will get some special notation, since we may have
ambiguous applications and we may want to name the result.
The extended notation will be `forward ... on ... as ...`.

TODO:
- Rewrite this document
- Add support for subexpression rewrites

## Dogma

1. Thou shalt not interpret thy functions
2. Thou shalt not extend this theorem prover
3. Thou shalt use only ASCII
4. Thou shalt not contribute to this prover with AI
5. Thou shalt not abbreviate thy keywords
