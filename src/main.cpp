#include <cadical.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

class Solver : public CaDiCaL::Solver {
public:
  Solver();

  std::optional<std::exception_ptr> py_error;
  std::unique_ptr<CaDiCaL::Terminator> terminator;
  std::unique_ptr<CaDiCaL::Learner> learner;

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
  terminator = std::make_unique<InterruptTerminator>(*this);
  connect_terminator(terminator.get());
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

  auto cls = py::class_<Solver>(m, "Solver");

  cls.def(py::init());
  cls.def_property_readonly_static(
      "signature", [](py::object) { return Solver::signature(); });
  cls.def("add", &Solver::add);
  cls.def("add_clause", [](Solver &self, py::iterable it) {
    for (py::iterator::reference lit : it) {
      self.add(lit.cast<int>());
    }
    self.add(0);
  });
  cls.def("add_clauses", [](Solver &self, py::iterable it) {
    for (py::iterator::reference clause : it) {
      for (py::iterator::reference lit : *clause) {
        self.add(lit.cast<int>());
      }
      self.add(0);
    }
  });
  cls.def("assume", &Solver::assume);
  cls.def("solve", [](Solver &self) {
    int result = self.solve();
    self.check_exception();
    return result;
  });
  cls.def("val", &Solver::val);
  cls.def("failed", &Solver::failed);
  cls.def("connect_interrupt_terminator",
      [](Solver &self, std::function<bool()> callback) {
        self.disconnect_terminator();
        self.terminator = std::make_unique<InterruptTerminator>(self);
        self.connect_terminator(self.terminator.get());
      });
  cls.def("connect_terminator", [](Solver &self,
                                    std::function<bool()> callback) {
    self.disconnect_terminator();
    self.terminator = std::make_unique<Terminator>(self, std::move(callback));
    self.connect_terminator(self.terminator.get());
  });
  cls.def("disconnect_terminator", [](Solver &self) {
    self.disconnect_terminator();
    self.terminator = nullptr;
  });
  cls.def("connect_learner",
      [](Solver &self, std::function<bool(int)> learning_callback,
          std::function<void(int)> learn_callback) {
        self.disconnect_learner();
        self.learner = nullptr;
        self.connect_learner(new Learner(
            self, std::move(learning_callback), std::move(learn_callback)));
      });
  cls.def("disconnect_learner", [](Solver &self) {
    self.disconnect_learner();
    self.learner = nullptr;
  });
  cls.def("lookahead", [](Solver &self) {
    int result = self.lookahead();
    self.check_exception();
    return result;
  });
  cls.def("generate_cubes", [](Solver &self, int depth) {
    Solver::CubesWithStatus result = self.generate_cubes(depth);
    // Terminating this doesn't work, but at least we can report the
    // exception at the end
    self.check_exception();
    return std::make_pair(result.status, result.cubes);
  });
  cls.def("reset_assumptions", &Solver::reset_assumptions);
  cls.def_property_readonly("state", [](Solver &self) {
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
  });
  cls.def_property_readonly("status", &Solver::status);
  cls.def_property_readonly_static(
      "version", [](py::object) { return Solver::version(); });
  cls.def("copy", &Solver::copy);
  cls.def("copy", [](Solver &self) {
    Solver *copy = new Solver;
    self.copy(*copy);
    return copy;
  });
  cls.def_property_readonly("vars", &Solver::vars);
  cls.def("reserve", &Solver::reserve);
  cls.def_static("is_valid_option", &Solver::is_valid_option);
  cls.def_static("is_preprocessing_option", &Solver::is_preprocessing_option);
  cls.def_static("is_valid_long_option", &Solver::is_valid_long_option);
  cls.def("get", &Solver::get);
  cls.def("prefix", &Solver::prefix);
  cls.def("set", &Solver::set);
  cls.def("set_long_option", &Solver::set_long_option);
  cls.def_static("is_valid_configuration", &Solver::is_valid_configuration);
  cls.def("configure", &Solver::configure);
  cls.def("optimize", &Solver::optimize);
  cls.def("limit", &Solver::limit);
  cls.def("is_valid_limit", &Solver::is_valid_limit);
  cls.def_property_readonly("active", &Solver::active);
  cls.def_property_readonly("redundant", &Solver::redundant);
  cls.def_property_readonly("irredundant", &Solver::irredundant);
  cls.def("simplify", [](Solver &self, int rounds) {
    int result = self.simplify(rounds);
    self.check_exception();
    return result;
  });
  cls.def("simplify", [](Solver &self) {
    int result = self.simplify();
    self.check_exception();
    return result;
  });
  cls.def("terminate", &Solver::terminate);
  cls.def("frozen", &Solver::frozen);
  cls.def("freeze", &Solver::freeze);
  cls.def("melt", &Solver::melt);
  cls.def("fixed", &Solver::fixed);
  cls.def("phase", &Solver::phase);
  cls.def("unphase", &Solver::unphase);
  cls.def("trace_proof", [](Solver &self, FILE *file, const char *name) {
    return self.trace_proof(file, name);
  });
  cls.def("trace_proof",
      [](Solver &self, const char *name) { return self.trace_proof(name); });
  cls.def("flush_proof_trace", &Solver::flush_proof_trace);
  cls.def("close_proof_trace", &Solver::close_proof_trace);
  cls.def_static("usage", &Solver::usage);
  cls.def_static("configurations", &Solver::configurations);
  cls.def("statistics", &Solver::statistics);
  cls.def("resources", &Solver::resources);
  cls.def("options", &Solver::options);
  cls.def("traverse_clauses",
      [](Solver &self, std::function<bool(const std::vector<int> &)> callback) {
        ClauseIterator it(self, std::move(callback));
        bool result = self.traverse_clauses(it);
        self.check_exception();
        return result;
      });
  cls.def("clauses", [](Solver &self) {
    CollectingClauseIterator it;
    self.traverse_clauses(it);
    return it.clauses;
  });
  cls.def("traverse_witnesses_backward",
      [](Solver &self, std::function<bool(
                           const std::vector<int> &, const std::vector<int> &)>
                           callback) {
        WitnessIterator it(self, std::move(callback));
        bool result = self.traverse_witnesses_backward(it);
        self.check_exception();
        return result;
      });
  cls.def("traverse_witnesses_forward",
      [](Solver &self, std::function<bool(
                           const std::vector<int> &, const std::vector<int> &)>
                           callback) {
        WitnessIterator it(self, std::move(callback));
        bool result = self.traverse_witnesses_forward(it);
        self.check_exception();
        return result;
      });
  cls.def("witnesses", [](Solver &self) {
    CollectingWitnessIterator it;
    self.traverse_witnesses_forward(it);
    return it.clauses;
  });
#if 0
// There doesn't seem to be a way to pass a FILE * from python, but there
// overloads below accept file paths, so this isn't essential.
  cls.def("read_dimacs",
      [](Solver &self, FILE *file, const char *name, int strict) {
        int vars = -1;
        const char *msg = self.read_dimacs(file, name, vars, strict);
        return std::make_pair(msg, vars);
      });
  cls.def("read_dimacs", [](Solver &self, FILE *file, const char *name) {
    int vars = -1;
    const char *msg = self.read_dimacs(file, name, vars, 1);
    return std::make_pair(msg, vars);
  });
#endif
  cls.def("read_dimacs", [](Solver &self, const char *name, int strict) {
    int vars = -1;
    const char *msg = self.read_dimacs(name, vars, strict);
    return std::make_pair(msg, vars);
  });
  cls.def("read_dimacs", [](Solver &self, const char *name) {
    int vars = -1;
    const char *msg = self.read_dimacs(name, vars, 1);
    return std::make_pair(msg, vars);
  });
  cls.def("read_dimacs_inccnf", [](Solver &self, const char *name, int strict) {
    int vars = -1;
    bool incremental = false;
    std::vector<int> cubes;
    const char *msg = self.read_dimacs(name, vars, strict, incremental, cubes);
    return std::make_tuple(msg, vars, incremental, cubes);
  });
  cls.def("read_dimacs_inccnf", [](Solver &self, const char *name) {
    int vars = -1;
    bool incremental = false;
    std::vector<int> cubes;
    const char *msg = self.read_dimacs(name, vars, 1, incremental, cubes);
    return std::make_tuple(msg, vars, incremental, cubes);
  });
  cls.def("write_dimacs", &Solver::write_dimacs);
  cls.def("write_dimacs",
      [](Solver &self, const char *path) { return self.write_dimacs(path); });
  cls.def("write_extension", &Solver::write_extension);
  // Skipping the build function, as it only supports output to stdout or stderr

#ifdef VERSION_INFO
  m.attr("__version__") = VERSION_INFO;
#else
  m.attr("__version__") = "dev";
#endif

  m.attr("UNSOLVED") = 0;
  m.attr("SATISFIABLE") = 10;
  m.attr("UNSATISFIABLE") = 20;
}
