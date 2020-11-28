#include <cadical.hpp>
#include <functional>
#include <optional>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

class Solver : public CaDiCaL::Solver {
public:
  Solver();

  std::optional<std::exception_ptr> py_error;

  void check_exception() {
    if (py_error) {
      std::exception_ptr p(std::move(*py_error));
      py_error.reset();
      std::rethrow_exception(std::move(p));
    };
  }
};

class InterruptTerminator : public CaDiCaL::Terminator {
public:
  Solver &solver;
  InterruptTerminator(Solver &solver) : solver(solver) {}
  virtual bool terminate() override final {
    try {
      if (PyErr_CheckSignals() != 0)
        throw py::error_already_set();
      return false;
    } catch (pybind11::error_already_set &e) {
      solver.py_error.emplace(std::current_exception());
      return true;
    }
  }
};

Solver::Solver() : CaDiCaL::Solver() {
  connect_terminator(new InterruptTerminator(*this));
}

class Terminator : public CaDiCaL::Terminator {
public:
  Solver &solver;
  std::function<bool()> callback;
  Terminator(Solver &solver, std::function<bool()> callback)
      : solver(solver), callback(std::move(callback)) {}
  virtual bool terminate() override final {
    try {
      return callback();
    } catch (pybind11::error_already_set &e) {
      solver.py_error.emplace(std::current_exception());
      return true;
    }
  }
};

class Learner : public CaDiCaL::Learner {
public:
  Solver &solver;
  std::function<bool(int)> learning_callback;
  std::function<void(int)> learn_callback;
  Learner(Solver &solver, std::function<bool(int)> learning_callback,
      std::function<void(int)> learn_callback)
      : solver(solver), learning_callback(std::move(learning_callback)),
        learn_callback(std::move(learn_callback)) {}
  virtual bool learning(int size) override final {
    try {
      return learning_callback(size);
    } catch (pybind11::error_already_set &e) {
      solver.py_error.emplace(std::current_exception());
      solver.terminate();
      return false;
    }
  }
  virtual void learn(int lit) override final {
    try {
      learn_callback(lit);
    } catch (pybind11::error_already_set &e) {
      solver.py_error.emplace(std::current_exception());
      solver.terminate();
    }
  }
};

class ClauseIterator : public CaDiCaL::ClauseIterator {
public:
  Solver &solver;
  std::function<bool(const std::vector<int> &)> callback;
  ClauseIterator(
      Solver &solver, std::function<bool(const std::vector<int> &)> callback)
      : solver(solver), callback(std::move(callback)) {}
  virtual bool clause(const std::vector<int> &clause) override final {
    try {
      return callback(clause);
    } catch (pybind11::error_already_set &e) {
      solver.py_error.emplace(std::current_exception());
      return false;
    }
  }
};

class CollectingClauseIterator : public CaDiCaL::ClauseIterator {
public:
  py::list clauses;
  virtual bool clause(const std::vector<int> &clause) override final {
    clauses.append(clause);
    return true;
  }
};

class WitnessIterator : public CaDiCaL::WitnessIterator {
public:
  Solver &solver;
  std::function<bool(const std::vector<int> &, const std::vector<int> &)>
      callback;
  WitnessIterator(Solver &solver,
      std::function<bool(const std::vector<int> &, const std::vector<int> &)>
          callback)
      : solver(solver), callback(std::move(callback)) {}
  virtual bool witness(const std::vector<int> &clause,
      const std::vector<int> &witness) override final {
    try {
      return callback(clause, witness);
    } catch (pybind11::error_already_set &e) {
      solver.py_error.emplace(std::current_exception());
      return false;
    }
  }
};

class CollectingWitnessIterator : public CaDiCaL::WitnessIterator {
public:
  py::list clauses;
  virtual bool witness(const std::vector<int> &clause,
      const std::vector<int> &witness) override final {
    clauses.append(std::make_pair(clause, witness));
    return true;
  }
};

PYBIND11_MODULE(pydical, m) {
  m.doc() = R"pbdoc(
        Pydical Python interface for the CaDiCaL SAT solver
    )pbdoc";

  py::class_<Solver>(m, "Solver")
      .def(py::init())
      .def_property_readonly_static(
          "signature", [](py::object) { return Solver::signature(); })
      .def("add", &Solver::add)
      .def("add_clause",
          [](Solver &self, py::iterable it) {
            for (py::iterator::reference lit : it) {
              self.add(lit.cast<int>());
            }
            self.add(0);
          })
      .def("extend",
          [](Solver &self, py::iterable it) {
            for (py::iterator::reference clause : it) {
              for (py::iterator::reference lit : *clause) {
                self.add(lit.cast<int>());
              }
              self.add(0);
            }
          })
      .def("assume", &Solver::assume)
      .def("solve",
          [](Solver &self) {
            int result = self.solve();
            self.check_exception();
            return result;
          })
      .def("val", &Solver::val)
      .def("failed", &Solver::failed)
      .def("connect_interrupt_terminator",
          [](Solver &self, std::function<bool()> callback) {
            self.connect_terminator(new InterruptTerminator(self));
          })
      .def("connect_terminator",
          [](Solver &self, std::function<bool()> callback) {
            self.connect_terminator(new Terminator(self, std::move(callback)));
          })
      .def("disconnect_terminator", &Solver::disconnect_terminator)
      .def("connect_learner",
          [](Solver &self, std::function<bool(int)> learning_callback,
              std::function<void(int)> learn_callback) {
            self.connect_learner(new Learner(
                self, std::move(learning_callback), std::move(learn_callback)));
          })
      .def("disconnect_learner", &Solver::disconnect_learner)
      .def("lookahead",
          [](Solver &self) {
            int result = self.lookahead();
            self.check_exception();
            return result;
          })
      .def("generate_cubes", [](Solver &self, int depth) {
        Solver::CubesWithStatus result = self.generate_cubes(depth);
        // Terminating this doesn't work, but at least we can report the
        // exception at the end
        self.check_exception();
        return std::make_pair(result.status, result.cubes);
      })
      .def("reset_assumptions", &Solver::reset_assumptions)
      .def_property_readonly("state",
          [](Solver &self) {
            switch (self.state()) {
            case CaDiCaL::INITIALIZING:
              return "INITIALIZING";
            case CaDiCaL::CONFIGURING:
              return "CONFIGURING";
            case CaDiCaL::UNKNOWN:
              return "UNKNOWN";
            case CaDiCaL::ADDING:
              return "ADDING";
            case CaDiCaL::SOLVING:
              return "SOLVING";
            case CaDiCaL::SATISFIED:
              return "SATISFIED";
            case CaDiCaL::UNSATISFIED:
              return "UNSATISFIED";
            case CaDiCaL::DELETING:
              return "DELETING";
            default:
              return "";
            }
          })
      .def_property_readonly("status", &Solver::status)
      .def_property_readonly_static(
          "version", [](py::object) { return Solver::version(); })
      .def("copy", &Solver::copy)
      .def("copy",
          [](Solver &self) {
            Solver *copy = new Solver;
            self.copy(*copy);
            return copy;
          })
      .def_property_readonly("vars", &Solver::vars)
      .def("reserve", &Solver::reserve)
      .def_static("is_valid_option", &Solver::is_valid_option)
      .def_static("is_preprocessing_option", &Solver::is_preprocessing_option)
      .def_static("is_valid_long_option", &Solver::is_valid_long_option)
      .def("get", &Solver::get)
      .def("prefix", &Solver::prefix)
      .def("set", &Solver::set)
      .def("set_long_option", &Solver::set_long_option)
      .def_static("is_valid_configuration", &Solver::is_valid_configuration)
      .def("configure", &Solver::configure)
      .def("optimize", &Solver::optimize)
      .def("limit", &Solver::limit)
      .def("is_valid_limit", &Solver::is_valid_limit)
      .def_property_readonly("active", &Solver::active)
      .def_property_readonly("redundant", &Solver::redundant)
      .def_property_readonly("irredundant", &Solver::irredundant)
      .def("simplify",
          [](Solver &self, int rounds) {
            int result = self.simplify(rounds);
            self.check_exception();
            return result;
          })
      .def("simplify",
          [](Solver &self) {
            int result = self.simplify();
            self.check_exception();
            return result;
          })
      .def("terminate", &Solver::terminate)
      .def("frozen", &Solver::frozen)
      .def("freeze", &Solver::freeze)
      .def("melt", &Solver::melt)
      .def("fixed", &Solver::fixed)
      .def("phase", &Solver::phase)
      .def("unphase", &Solver::unphase)
      .def("trace_proof",
          [](Solver &self, FILE *file, const char *name) {
            return self.trace_proof(file, name);
          })
      .def("trace_proof",
          [](Solver &self, const char *name) { return self.trace_proof(name); })
      .def("flush_proof_trace", &Solver::flush_proof_trace)
      .def("close_proof_trace", &Solver::close_proof_trace)
      .def_static("usage", &Solver::usage)
      .def_static("configurations", &Solver::configurations)
      .def("statistics", &Solver::statistics)
      .def("resources", &Solver::resources)
      .def("options", &Solver::options)
      .def("traverse_clauses",
          [](Solver &self,
              std::function<bool(const std::vector<int> &)> callback) {
            ClauseIterator it(self, std::move(callback));
            bool result = self.traverse_clauses(it);
            self.check_exception();
            return result;
          })
      .def("clauses",
          [](Solver &self) {
            CollectingClauseIterator it;
            self.traverse_clauses(it);
            return it.clauses;
          })
      .def("traverse_witnesses_backward",
          [](Solver &self, std::function<bool(const std::vector<int> &,
                               const std::vector<int> &)>
                               callback) {
            WitnessIterator it(self, std::move(callback));
            bool result = self.traverse_witnesses_backward(it);
            self.check_exception();
            return result;
          })
      .def("traverse_witnesses_forward",
          [](Solver &self, std::function<bool(const std::vector<int> &,
                               const std::vector<int> &)>
                               callback) {
            WitnessIterator it(self, std::move(callback));
            bool result = self.traverse_witnesses_forward(it);
            self.check_exception();
            return result;
          })
      .def("witnesses",
          [](Solver &self) {
            CollectingWitnessIterator it;
            self.traverse_witnesses_forward(it);
            return it.clauses;
          })
      .def("read_dimacs",
          [](Solver &self, FILE *file, const char *name, int strict) {
            int vars = -1;
            const char *msg = self.read_dimacs(file, name, vars, strict);
            return std::make_pair(msg, vars);
          })
      .def("read_dimacs",
          [](Solver &self, FILE *file, const char *name) {
            int vars = -1;
            const char *msg = self.read_dimacs(file, name, vars, 1);
            return std::make_pair(msg, vars);
          })
      .def("read_dimacs",
          [](Solver &self, const char *name, int strict) {
            int vars = -1;
            const char *msg = self.read_dimacs(name, vars, strict);
            return std::make_pair(msg, vars);
          })
      .def("read_dimacs",
          [](Solver &self, const char *name) {
            int vars = -1;
            const char *msg = self.read_dimacs(name, vars, 1);
            return std::make_pair(msg, vars);
          })
      .def("read_dimacs_inccnf",
          [](Solver &self, const char *name, int strict) {
            int vars = -1;
            bool incremental = false;
            std::vector<int> cubes;
            const char *msg = self.read_dimacs(name, vars, strict, incremental, cubes);
            return std::make_tuple(msg, vars, incremental, cubes);
          })
      .def("read_dimacs_inccnf",
          [](Solver &self, const char *name) {
            int vars = -1;
            bool incremental = false;
            std::vector<int> cubes;
            const char *msg = self.read_dimacs(name, vars, 1, incremental, cubes);
            return std::make_tuple(msg, vars, incremental, cubes);
          })
      .def("write_dimacs", &Solver::write_dimacs)
      .def("write_dimacs",
          [](Solver &self,
              const char *path) { return self.write_dimacs(path); })
      .def("write_extension", &Solver::write_extension)
      .def_static("build", &Solver::build)
      .def_static("build", [](py::object, FILE *file) { Solver::build(file); })
      /**/
      ;

#ifdef VERSION_INFO
  m.attr("__version__") = VERSION_INFO;
#else
  m.attr("__version__") = "dev";
#endif
}
