cmake_minimum_required(VERSION 3.4)

project(pydical)

find_package(pybind11 REQUIRED)

set(CADICAL_SRC
    cadical/src/analyze.cpp
    cadical/src/arena.cpp
    cadical/src/assume.cpp
    cadical/src/averages.cpp
    cadical/src/backtrack.cpp
    cadical/src/backward.cpp
    cadical/src/bins.cpp
    cadical/src/block.cpp
    cadical/src/ccadical.cpp
    cadical/src/checker.cpp
    cadical/src/clause.cpp
    cadical/src/collect.cpp
    cadical/src/compact.cpp
    cadical/src/condition.cpp
    cadical/src/config.cpp
    cadical/src/contract.cpp
    cadical/src/cover.cpp
    cadical/src/decide.cpp
    cadical/src/decompose.cpp
    cadical/src/deduplicate.cpp
    cadical/src/elim.cpp
    cadical/src/ema.cpp
    cadical/src/extend.cpp
    cadical/src/external.cpp
    cadical/src/file.cpp
    cadical/src/flags.cpp
    cadical/src/format.cpp
    cadical/src/gates.cpp
    cadical/src/instantiate.cpp
    cadical/src/internal.cpp
    cadical/src/ipasir.cpp
    cadical/src/limit.cpp
    cadical/src/logging.cpp
    cadical/src/lookahead.cpp
    cadical/src/lucky.cpp
    cadical/src/message.cpp
    cadical/src/minimize.cpp
    cadical/src/occs.cpp
    cadical/src/options.cpp
    cadical/src/parse.cpp
    cadical/src/phases.cpp
    cadical/src/probe.cpp
    cadical/src/profile.cpp
    cadical/src/proof.cpp
    cadical/src/propagate.cpp
    cadical/src/queue.cpp
    cadical/src/random.cpp
    cadical/src/reduce.cpp
    cadical/src/rephase.cpp
    cadical/src/report.cpp
    cadical/src/resources.cpp
    cadical/src/restart.cpp
    cadical/src/restore.cpp
    cadical/src/score.cpp
    cadical/src/signal.cpp
    cadical/src/solution.cpp
    cadical/src/solver.cpp
    cadical/src/stats.cpp
    cadical/src/subsume.cpp
    cadical/src/terminal.cpp
    cadical/src/ternary.cpp
    cadical/src/tracer.cpp
    cadical/src/transred.cpp
    cadical/src/util.cpp
    cadical/src/var.cpp
    cadical/src/version.cpp
    cadical/src/vivify.cpp
    cadical/src/walk.cpp
    cadical/src/watch.cpp
)

file(STRINGS "cadical/VERSION" CADICAL_VERSION)

set_property(
    SOURCE ${CADICAL_SRC}
    PROPERTY COMPILE_DEFINITIONS
    NBUILD VERSION="${CADICAL_VERSION}"
)

pybind11_add_module(pydical
    src/main.cpp
    ${CADICAL_SRC}
)

target_include_directories(pydical PRIVATE cadical/src)

set_property(TARGET pydical PROPERTY CXX_STANDARD 17)
