#pragma once
#include "Context.hpp"
#include "Node.hpp"
#include "RegionLikeImpl.hpp"

namespace shader::ir {
template <typename BuilderT, typename ImplT> struct BuilderFacade {
  ImplT &instance() {
    return *static_cast<ImplT *>(static_cast<BuilderT *>(this));
  }
  Context &getContext() { return instance().getContext(); }

  Node getInsertionStorage() { return instance().getInsertionStorage(); }
  template <typename T, typename... ArgsT>
    requires requires {
      typename T::underlying_type;
      requires std::is_constructible_v<typename T::underlying_type, ArgsT...>;
      requires std::is_base_of_v<NodeImpl, typename T::underlying_type>;
    }
  T create(ArgsT &&...args) {
    return instance().template create<T>(std::forward<ArgsT>(args)...);
  }
};

template <template <typename> typename... InterfaceTs>
class Builder : public InterfaceTs<Builder<InterfaceTs...>>... {
  Context *mContext{};
  RegionLike mInsertionStorage;
  Instruction mInsertionPoint;

public:
  Builder() = default;
  Builder(Context &context) : mContext(&context) {}

  static Builder createInsertAfter(Context &context, Instruction point) {
    auto result = Builder(context);
    result.mInsertionStorage = point.getParent();
    result.mInsertionPoint = point;
    return result;
  }

  static Builder createInsertBefore(Context &context, Instruction point) {
    auto result = Builder(context);
    result.mInsertionStorage = point.getParent();
    result.mInsertionPoint = point.getPrev().cast<Instruction>();
    return result;
  }

  static Builder createAppend(Context &context, RegionLike storage) {
    auto result = Builder(context);
    result.mInsertionStorage = storage;
    result.mInsertionPoint = storage.getLast().cast<Instruction>();
    return result;
  }

  static Builder createPrepend(Context &context, RegionLike storage) {
    auto result = Builder(context);
    result.mInsertionStorage = storage;
    result.mInsertionPoint = nullptr;
    return result;
  }

  Context &getContext() { return *mContext; }
  RegionLike getInsertionStorage() { return mInsertionStorage; }
  Instruction getInsertionPoint() { return mInsertionPoint; }
  void setInsertionPoint(Instruction inst) { mInsertionPoint = inst; }

  template <typename T, typename... ArgsT>
    requires requires {
      typename T::underlying_type;
      requires std::is_constructible_v<typename T::underlying_type, ArgsT...>;
      requires std::is_base_of_v<NodeImpl, typename T::underlying_type>;
    }
  T create(ArgsT &&...args) {
    auto result = getContext().template create<T>(std::forward<ArgsT>(args)...);
    using InstanceType = typename T::underlying_type;
    getInsertionStorage().insertAfter(getInsertionPoint(), result);
    if constexpr (requires { mInsertionPoint = Instruction(result); }) {
      mInsertionPoint = Instruction(result);
    }
    return result;
  }
};
} // namespace ir
