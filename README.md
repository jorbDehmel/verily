
# Verily: A Dogmatic Theorem Prover

![The Verily logo: A "V" with a horizontal line through it](verily-highlighting/logo.png)

Jordan Dehmel, MIT License, 2025-present

## About

Verily is a theorem prover based on term rewriting systems.
It is *not* based on the Curry-Howard isomorphism (though it
contains and is contained by it). Notably, `verily` has no type
system. The goal is to be extremely quick and lightweight: The
assembly language of theorem proving. Verily is built around the
idea of deductive calculi, rather than any specific deductive
calculus. As such, there are very few tactics or even logical
constructs built-in. If you want a fully-featured theorem
prover, you must accept the numerous axioms and points of
failure that come with it.

**Everything is uninterpreted!** Though operators like `+`, `*`,
and even `==` can be added via the `notation` keyword,
**they don't do anything!** They are just ASTs! If you want them
to do anything, you have to add that functionality
axiomatically. There is only one exception:
`syntactically_equivalent a b` is provable **iff** `a` is
syntactically equivalent to `b` (and
`not_syntactically_equivalent a b` likewise). I have made it
purposefully verbose because you need to understand exactly what
it is doing! It is not the same as `==` (which doesn't even
exist by default). For instance, even if we have declared
`a == b` axiomatically, we will never prove
`syntactically_equivalent a b`, because the symbols `a` and `b`,
regardless of what they may model, are not syntactically
equivalent.

This code is written for linux, but the core theorem prover will
probably work no matter the platform. The package manager will
almost certainly not work on EG Windows.

## Rules for Contributing

1. Thou shalt not interpret functions
2. Thou shalt not endow this theorem prover with external
  resources (no internal interfaces to other provers)
3. Thou shalt not contribute to this prover with AI (No
  exceptions: not even a little bit, not even tangentially, not
  even "based on a conversation with Claude". I don't care if
  you use AI to write code in verily, but never use it to
  contribute to the tool itself)
4. Thou shalt not covet the features of other theorem provers
5. Thou shalt avoid bloat and feature creep

## This Repo

This repo contains some examples, a verily CLI, some tests, and
a verily syntax highlighting extension for `vscode`.

To build the CLI, just run `make` from the root of this repo.
This will create `./verily.out`, which is the executable. Run
`./verily.out --help` for help. If you run
`./verily.out file_name.verily`, it will check that file. If you
run `./verily.out`, it will enter CLI mode.

To install the `vscode` syntax highlighting extension, change
directories to `verily-highlighting`. Then, run
`npx @vscode/vsce package` to build the extension. This will
produce a local file ending in `.vsix`. If this file were named
`NAME_GOES_HERE.vsix`, you would then run
`code --install-extension NAME_GOES_HERE.vsix`. You must then
reload your `vscode` window (ctrl+shift+p
'Developer: Reload Window') for the extension to start
syntax-highlighting any open files with the extension `.verily`.

## Syntax

Many comment styles are supported.

```verily
# I am a comment (python-style)
-- I am a comment (lean-style)
// I am a comment (C++-style)
```

There are no block comments.

### Meta

You can include other files by saying
`include "path_to_other_file.verily";`. You can modify settings
by saying `option "name_of_setting=value_of_setting";`. Settings
should not usually be necessary.

You can introduce operators as follows, using the "notation"
keyword.

```verily
notation in:
  over A, B
  given A in B
  deduce (in A B)
;

notation not:
  over A
  given not, A
  deduce (not A)
;

notation prime:
  over A
  given A'
  deduce (prime A)
;
```

This means that `a in b` will parse to `(in a b)`, `a'` will
parse to `(prime a)`, and `not a` will parse to `(not a)`. When
not specified, `a b c` would parse to the list `(_ a b c)`.
There are no applications by default.

You can also specify where in the order-of-operations an
operator should go.

```verily
notation append:
  over A, B
  given A ++ B
  deduce (concat A B)
  before add
;
```

This would cause `c + a ++ b` to parse to the same thing
as `c + (a ++ b)`. If no `before` is specified, the notation
goes at the end of the order-of-operations (before no
operations). Note that we say `before add`, not `before +`.
This also requires that `add` is a notation which is already
added!

Note that `notation` takes almost the same form as `axiom`: This
is not a coincidence! They actually refer to the same operation,
but `notation` operates on the program AST and `axiom` acts on
the proof AST.

### Axioms (Schemas)

There are no axioms in verily, only axiom schemas / rules.
They take the following form.

```verily
axiom name:
  over free_var_1, free_var_2
  given antecedent_1, antecedent_2, antecedent_3
  deduce consequent
;

# It's also legal to leave out the name
axiom:
  over A
  given B
  deduce A
;

# And/or the over and given clauses
axiom:
  given B
  deduce A
;

axiom name:
  over A
  deduce A
;

axiom: deduce A;
```

There must be one and only one `deduce` item. Semicolons mark
the end of expressions, but are not strictly necessary if the
end of the expression is otherwise determinable.

### Theorems

Theorems are just derived rules. They take mostly the same
syntax, but with a `by { ... }` clause on the end containing a
proof.

```verily
theorem name:
  over A, B
  given h1, h2
  deduce C
by {
  # Proof goes here
}
```

After they are proven, you can apply them just like rules. The
only difference between them and rule is that theorems do not
count as axioms.

### Proofs

A proof, which can only appear in the `by` clause of a theorem,
has a few legal statements.

1. `bw rule_name_here` / `backward rule_name_here`
2. `wts new_goal_here { proof_of_new_goal_here }`
3. `fw rule_name_here` / `forward rule_name_here`
4. `sweep num_of_passes` / `sweep inf`
5. `have thm_goes_here as name`
6. `rw rule_name_here` / `rewrite rule_name_here`
7. `theorem ...` (same as normal)
8. `use ... for ... in ... as ...`
9. `sorry`

`forward` and `fw` are interchangeable, as are `backward` and
`bw`, and `rewrite` and `rw`. `wts` stands for "Want To Show".
`fw` has a few variants, listed below.

`have thm as name` just finds the **existing** proof of thm and
creates a new theorem named `name` with that same proof. This
is useful for hypothesis (`given` clauses).

```verily
fw rule_name
fw rule_name on antecedent_1
fw rule_name on antecedent_1, antecedent_2, antecedent_3
fw rule_name as result_name
fw rule_name on a1, a2 as result_name

-- Same as 'fw rule_name on foo as foo'
fw rule_name at foo
```

Unless the rule has no antecedents, you probably want to apply
it to something. `bw` does not have any variants! Only `bw name`
is legal!

`fw ... on ... as ...` pattern-matches the given rule's
antecedents with the provided arguments, then adds the result
as a theorem. It never changes the goal. `fw ... at ...` works
as mentioned above. `rw` has the same syntax as `fw`, but it
can rewrite subexpressions instead of the entire subexpression.
If you leave one or more arguments as `_`, the result will be
a new rule taking in those arguments. This is called a partial
application.

`bw rule_name` pattern-matches the current goal with the
consequent of the given rule, then adds all the resulting
antecedents as new goals (closing the old goal). It can change
the goal. An error will be raised if not all free variables in
the rule can be inferred.

`wts a { b }` introduces a goal `a` (it can be new, one that
already exists, or even the current goal), then checks that `b`
is a proof of `a`. If it is, `a` is eliminated as a goal and
added as a theorem. It can change the goal.

`sweep x`, where `x` is a natural number, synthesizes **all**
(exponentially many) theorems reachable within `x` forward
applications. `sweep inf` does this forever (or until no more
theorems can be derived or the *current* goal is solved). As
the name suggests, `sweep inf` can run forever if your TRS is
non-normalizing and the goal is unprovable. If the system is
normalizing or the goal is provable, `sweep inf` will always
halt (though it takes exponentially long WRT the length of the
proof). `sweep` never changes the goal. You should never have
`sweep` statements in a finished proof! You should only ever use
it to discover proofs that you can then manually insert!

If you are within a proof and want to synthesize a new rewrite
rule with antecedents and/or generics, you can use a
`theorem ...` (where `theorem` is interchangeable with `lemma`)
statement. It takes the same syntax as usual, and inherits any
assumptions that are currently active. Once proven, the theorem
is added as a new rewrite rule.

The statement `use exp_1 as exp_2 in name_1 as name_2` is a
partial instantiation: `exp_2` is a generic in rule `name_1`
which will be replaced by `exp_1`. The result will then be
saved as the new rule `name_2`. You can chain `... for ...`
clauses like `use foo as fizz, flim as flam in ... as ...`.
This is useful because rules are not allowed to have remaining
free variables (1) to count as theorems, and (2) after `bw`
applications.

`sorry` has the same functionality it does in lean: It closes
the current goal, no questions asked. It is only to be used as
a placeholder.

## Packages and `hark`

`verily` is the actual theorem prover, but `hark` is the CLI
tool for managing packages and versions. Use `hark --help` to
see more information. Packages are stored in `/usr/lib/verily`.
Package information is never duplicated (looking at you,
`lean4`), and there are no virtual environments. If we are
trying to find package `name` with version 1.2.3 for verily
version 3.2.1, we would look in
`/usr/lib/verily/3_2_1/name/1_2_3/` for files. We can impose
package version constraints via:

```verily
# Requires version 1.2.3 of name OR HIGHER. No error will be
# thrown here if no such packages exist.
option "require:name>=1.2.3";

# Note: We have to add the requirements before we access the
# package, or else it will just use the latest version installed

# Resolves to `/usr/lib/verily/3_2_1/name/1_2_3/foo.verily`.
# If no version of package 'name' exists satisfying our demands,
# an error would now be thrown.
include "name/foo.verily";

# Special case: Asserts bounds on the current version of verily.
# An error will be thrown if these fail.
option "require:verily<1";
option "require:verily>0";
```

Only `<=`, `>=`, and `==` are currently supported in `require`
statements. Packages can be added using
`hark --add name version git-repo-url` or
`hark --add-local name version path/to/package` and removed via
`hark --remove name version`. You will probably need `sudo`
privileges.

You can manage verily versions via `hark --ls-versions`,
`hark --add-version version url`,
`hark --remove-version version`, and
`hark --set-version version`.

## Vetoed Suggestions:

- Alternative functional theorem / axiom syntax
- Unified proof and "glue" syntax
- `suppose` proof statements for adding axioms on the fly

## Future

- It might be useful to have a `hint` keyword that shows all
  possible backwards goal groups.
- `sweep` calls should print the `fw`-only proofs they discover.
- The secondary proof checking system (for EG json) is *very*
  broken, and needs a complete rewrite.
- We should add a `verily-package.json` file for version info
  so we don't have to add it on the fly. We will use JSON
  because I think TOML and YAML are dumb. I am hesitant to add
  a JSON parser dependency to this project, though, especially
  since I've been using old-school `make`.
- A theorem database via EG `sqlite3` would be helpful for very
  large projects
