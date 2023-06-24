#pragma once

namespace amdgpu::shader {
enum class FragmentTerminator {
  None,
  EndProgram,
  CallToReg,
  BranchToReg,
  Branch,
};
}
