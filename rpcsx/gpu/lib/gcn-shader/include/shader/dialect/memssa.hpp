#pragma once

#include "../ir/Block.hpp"
#include "../ir/Builder.hpp"
#include "../ir/Value.hpp"
#include "../ir/ValueImpl.hpp"

namespace shader::ir::memssa {
enum Op {
  OpVar,
  OpDef,
  OpPhi,
  OpUse,
  OpBarrier,
  OpJump,
  OpExit,

  OpCount,
};

template <typename BaseT> struct BaseImpl : BaseT {
  Instruction link;

  using BaseT::BaseT;
  using BaseT::operator=;

  void print(std::ostream &os, NameStorage &ns) const override {
    BaseT::print(os, ns);

    if (link) {
      os << " : ";
      link.print(os, ns);
    }
  }
};

template <typename ImplT, template <typename> typename BaseT>
struct BaseWrapper : BaseT<ImplT> {
  using BaseT<ImplT>::BaseT;
  using BaseT<ImplT>::operator=;

  Instruction getLinkedInst() const { return this->impl->link; }
};

struct DefImpl : BaseImpl<ValueImpl> {
  using BaseImpl::BaseImpl;
  using BaseImpl::operator=;

  Node clone(Context &context, CloneMap &map) const override;
};
struct UseImpl : BaseImpl<InstructionImpl> {
  using BaseImpl::BaseImpl;
  using BaseImpl::operator=;

  Node clone(Context &context, CloneMap &map) const override;
};
struct VarImpl : BaseImpl<ValueImpl> {
  using BaseImpl::BaseImpl;
  using BaseImpl::operator=;

  Node clone(Context &context, CloneMap &map) const override;
};
struct PhiImpl : DefImpl {
  using DefImpl::DefImpl;
  using DefImpl::operator=;

  Node clone(Context &context, CloneMap &map) const override;
};

using Use = BaseWrapper<UseImpl, InstructionWrapper>;
using Var = BaseWrapper<VarImpl, ValueWrapper>;

template <typename ImplT> struct DefWrapper : BaseWrapper<ImplT, ValueWrapper> {
  using BaseWrapper<ImplT, ValueWrapper>::BaseWrapper;
  using BaseWrapper<ImplT, ValueWrapper>::operator=;

  void addVariable(Var variable) {
    this->addOperand(variable);

    std::vector<Var> workList;

    for (auto &comp : variable.getOperands()) {
      auto compVar = comp.getAsValue().staticCast<Var>();
      this->addOperand(compVar);

      if (compVar.getOperandCount() > 1) {
        workList.push_back(compVar);
      } else if (compVar.getOperandCount() == 1) {
        this->addOperand(compVar.getOperand(0).getAsValue().staticCast<Var>());
      }
    }

    while (!workList.empty()) {
      auto var = workList.back();
      workList.pop_back();

      for (auto &comp : var.getOperands()) {
        auto compVar = comp.getAsValue().staticCast<Var>();
        this->addOperand(compVar);

        if (compVar.getOperandCount() > 1) {
          workList.push_back(var);
        } else if (compVar.getOperandCount() == 1) {
          this->addOperand(
              compVar.getOperand(0).getAsValue().staticCast<Var>());
        }
      }
    }
  }

  Var getRootVar() {
    return this->getOperand(0).getAsValue().template staticCast<Var>();
  }

  Var getVar(std::size_t index) {
    return this->getOperand(index).getAsValue().template staticCast<Var>();
  }
};

struct ScopeImpl : BaseImpl<ir::BlockImpl> {
  using BaseImpl::BaseImpl;
  using BaseImpl::operator=;

  Node clone(Context &context, CloneMap &map) const override;
};

template <typename ImplT> struct ScopeWrapper;

using Scope = ScopeWrapper<ScopeImpl>;
using Def = DefWrapper<DefImpl>;

template <typename ImplT> struct BarrierWrapper : DefWrapper<ImplT> {
  using DefWrapper<ImplT>::DefWrapper;
  using DefWrapper<ImplT>::operator=;
};

using Barrier = BarrierWrapper<PhiImpl>;

template <typename ImplT>
struct ScopeWrapper : BaseWrapper<ImplT, ir::BlockWrapper> {
  using BaseWrapper<ImplT, ir::BlockWrapper>::BaseWrapper;
  using BaseWrapper<ImplT, ir::BlockWrapper>::operator=;

  Scope getSingleSuccessor() {
    if (this->empty()) {
      return {};
    }
    auto terminator = this->getLast();
    if (terminator.getKind() != Kind::MemSSA || terminator.getOp() != OpJump) {
      return {};
    }
    if (terminator.getOperandCount() != 1) {
      return {};
    }

    return terminator.getOperand(0).getAsValue().template cast<Scope>();
  }

  std::vector<Scope> getSuccessors() {
    if (this->empty()) {
      return {};
    }
    auto terminator = this->getLast();
    if (terminator.getKind() != Kind::MemSSA || terminator.getOp() != OpJump) {
      return {};
    }

    std::vector<Scope> result;
    result.reserve(terminator.getOperandCount());
    for (auto &successor : terminator.getOperands()) {
      if (auto block = successor.getAsValue().template cast<Scope>()) {
        result.push_back(block);
      }
    }
    return result;
  }

  auto getPredecessors() {
    std::set<Scope> predecessors;
    for (auto &use : this->getUseList()) {
      if (use.user != OpJump) {
        continue;
      }

      if (auto userParent = use.user.getParent().template cast<Scope>()) {
        predecessors.insert(userParent);
      }
    }
    return predecessors;
  }

  auto getSinglePredecessor() {
    Scope predecessor;

    for (auto &use : this->getUseList()) {
      if (use.user != OpJump) {
        continue;
      }

      if (auto userParent = use.user.getParent().template cast<Scope>()) {
        if (predecessor == nullptr) {
          predecessor = userParent;
        } else if (predecessor != userParent) {
          return Scope(nullptr);
        }
      }
    }

    return predecessor;
  }

  Def findVarDef(Var var, Instruction point = nullptr) {
    if (point == nullptr) {
      point = this->getLast();
    }

    std::optional<std::set<Var>> compList;

    auto buildMatchList = [&] {
      std::set<Var> result;
      std::vector<Var> workList;

      for (auto comp : var.getOperands()) {
        auto compVar = comp.getAsValue().staticCast<Var>();
        result.insert(compVar);

        if (compVar.getOperandCount() > 1) {
          workList.push_back(compVar);
        } else if (compVar.getOperandCount() == 1) {
          result.insert(compVar.getOperand(0).getAsValue().staticCast<Var>());
        }
      }

      while (!workList.empty()) {
        auto var = workList.back();
        workList.pop_back();

        for (auto comp : var.getOperands()) {
          auto compVar = comp.getAsValue().staticCast<Var>();
          result.insert(compVar);

          if (compVar.getOperandCount() > 1) {
            workList.push_back(compVar);
          } else if (compVar.getOperandCount() == 1) {
            result.insert(compVar.getOperand(0).getAsValue().staticCast<Var>());
          }
        }
      }

      return result;
    };

    for (auto child : revRange(point)) {
      if (child.getKind() != Kind::MemSSA) {
        continue;
      }

      if (child.getOp() == OpDef || child.getOp() == OpPhi) {
        if (child.getOperand(0) == var) {
          return child.template staticCast<Def>();
        }

        if (!compList) {
          compList = buildMatchList();
        }

        if (compList->empty()) {
          continue;
        }

        if (compList->contains(
                child.getOperand(0).getAsValue().staticCast<Var>())) {
          return child.template staticCast<Def>();
        }
      }

      if (child.getOp() == OpBarrier) {
        // barrier is definition for everything
        return child.template staticCast<Def>();
      }
    }

    return {};
  }
};

template <typename ImplT> struct PhiWrapper : ValueWrapper<ImplT> {
  using ValueWrapper<ImplT>::ValueWrapper;
  using ValueWrapper<ImplT>::operator=;

  void addValue(Scope scope, Def def) {
    this->addOperand(scope);
    this->addOperand(def);
  }

  // Set value for specified block or add new node
  // Returns true if node was added
  bool setValue(Scope pred, Def def) {
    for (std::size_t i = 1, end = this->getOperandCount(); i < end; i += 2) {
      if (pred == this->getOperand(i).getAsValue()) {
        this->replaceOperand(i + 1, def);
        return false;
      }
    }

    addValue(pred, def);
    return true;
  }

  Def getDef(Scope pred) {
    for (std::size_t i = 1, end = this->getOperandCount(); i < end; i += 2) {
      if (pred == this->getOperand(i).getAsValue()) {
        return this->getOperand(i + 1).getAsValue().template staticCast<Def>();
      }
    }

    return {};
  }

  bool empty() { return this->getOperandCount() < 2; }

  Def getUniqDef() {
    if (empty()) {
      return {};
    }

    Def result = this->getOperand(2).getAsValue().template staticCast<Def>();

    for (std::size_t i = 4, end = this->getOperandCount(); i < end; i += 2) {
      if (this->getOperand(i) != result) {
        return {};
      }
    }

    return result;
  }

  Var getVar() {
    return this->getOperand(0).getAsValue().template staticCast<Var>();
  }
};

using Phi = PhiWrapper<PhiImpl>;

template <typename ImplT>
struct Builder : BuilderFacade<Builder<ImplT>, ImplT> {
  Def createDef(Instruction defInst, Var var) {
    auto result =
        this->template create<Def>(defInst.getLocation(), Kind::MemSSA, OpDef);
    result.impl->link = defInst;
    result.addOperand(var);
    return result;
  }

  Scope createScope(ir::Instruction labelInst) {
    Scope result = this->template create<Scope>(labelInst.getLocation());
    result.impl->link = labelInst;
    return result;
  }

  Phi createPhi(Var var) {
    auto result =
        this->template create<Phi>(var.getLocation(), Kind::MemSSA, OpPhi);
    result.addOperand(var);
    return result;
  }

  Use createUse(ir::Instruction useInst) {
    Use result =
        this->template create<Use>(useInst.getLocation(), Kind::MemSSA, OpUse);
    result.impl->link = useInst;
    return result;
  }

  Use createUse(ir::Instruction useInst, Def def) {
    auto result = createUse(useInst);
    result.addOperand(def);
    return result;
  }

  Var createVar(ir::Instruction varInst) {
    Var result =
        this->template create<Var>(varInst.getLocation(), Kind::MemSSA, OpVar);
    result.impl->link = varInst;
    return result;
  }

  Barrier createBarrier(ir::Instruction barrierInst) {
    Barrier result = this->template create<Barrier>(barrierInst.getLocation(),
                                                    Kind::MemSSA, OpBarrier);
    result.impl->link = barrierInst;
    return result;
  }

  Instruction createJump(Location loc) {
    return this->template create<Instruction>(loc, Kind::MemSSA, OpJump);
  }

  Instruction createExit(Location loc) {
    return this->template create<Instruction>(loc, Kind::MemSSA, OpExit);
  }
};

inline const char *getInstructionName(unsigned op) {
  switch (op) {
  case OpVar:
    return "var";
  case OpDef:
    return "def";
  case OpPhi:
    return "phi";
  case OpUse:
    return "use";
  case OpBarrier:
    return "barrier";
  case OpJump:
    return "jump";
  case OpExit:
    return "exit";
  }
  return nullptr;
}
} // namespace shader::ir::memssa
