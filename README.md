# Pydical - Python wrapper for the CaDiCaL SAT solver

This is yet another Python wrapper for the [CaDiCaL SAT solver][1]. It differs
from other wrappers (that I'm aware of) by providing coverage for the whole C++
API of CaDiCaL, including various callbacks. It is implemented using
[pybind11][2].

I've written it for my own use and am sharing it in case someone else has
similar requirements. Currently I have no intention to commit on maintaining
this long term.

The provided Python API closely mirrors the solver's C++ API documented in
[`cadical/src/cadical.hpp`](cadical/src/cadical.hpp). On top of that it adds a
few convenience features like `Solver.add_clause` and `Solver.add_clauses` to
add one or multiple clauses from a Python iterable.

By default it also installs a termination callback that checks for
`KeyboardInterrupts` to terminate solving. All callbacks propagate exceptions
and if an exception occurs, terminate solving.

Right now there is no Python specific documentation. Given that it closely
follows the C++ API, I don't expect that to be a problem, although it would be
nicer to at least have docstrings on all methods.

In the near future I plan to extend CaDiCaL's API to expose some more
internals. From that point on Pydical will use my own fork of CaDiCaL, as
CaDiCaL does not accept external contributions.

# Building and Installing

Requires `cmake` and `pybind11` as build dependencies. CaDiCaL is built and
included with Pydical's Python extension. Pydical can be built and installed
using the `pip install .` command. For development, `cmake` can be used stand
alone to build the `pydical` Python extension which can be tested by pointing
`PYTHONPATH` to cmake's build directory. Do not forget to set cmake's build
type to `Release` if you want a fast solver. I ran into multiple issues when
using Python's "editiable install" devlopment feature for this project, so that
is currently not supported.

I use Python mainly for prototyping ideas, so my Python packaging experience is
largely non-existent. I have also only tested building this with Python 3.8 on
x86_64 Linux.

Feel free to open issues or pull-requests for improvements in either area.

# Example

See [`examples/simplify_random.py`](examples/simplify_random.py) for a script
that generates a random satisfiable instance by incrementally adding clauses.
It uses callbacks to print out small clauses learned during those incremental
solves. It then simplifies the formula using a second solver instance and
CaDiCaL's simplify call and outputs the resulting simplified formula.

[1]: http://fmv.jku.at/cadical/
[2]: https://github.com/pybind/pybind11
