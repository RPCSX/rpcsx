#pragma once

#include "util/endian.hpp"
#include "Emu/RSX/Program/RSXVertexProgram.h"
#include "Emu/RSX/Program/RSXFragmentProgram.h"
#include "Emu/RSX/Program/ProgramStateCache.h"
#include "Emu/RSX/Program/ShaderParam.h"
#include "util/File.h"
#include <cstddef>
#include <span>

#ifndef WITHOUT_OPENGL
#include "Emu/RSX/GL/GLVertexProgram.h"
#include "Emu/RSX/GL/GLFragmentProgram.h"
#endif

using CGprofile = be_t<u32>;
using CGbool = be_t<s32>;
using CGresource = be_t<u32>;
using CGenum = be_t<u32>;
using CGtype = be_t<u32>;
using CGbitfield = be_t<u32>;
using CGbitfield16 = be_t<u16>;
using CGint = be_t<s32>;
using CGuint = be_t<u32>;

using CgBinaryOffset = CGuint;
using CgBinarySize = CgBinaryOffset;
using CgBinaryEmbeddedConstantOffset = CgBinaryOffset;
using CgBinaryFloatOffset = CgBinaryOffset;
using CgBinaryStringOffset = CgBinaryOffset;
using CgBinaryParameterOffset = CgBinaryOffset;

// fragment programs have their constants embedded in the microcode
struct CgBinaryEmbeddedConstant
{
	be_t<u32> ucodeCount;     // occurrences
	be_t<u32> ucodeOffset[1]; // offsets that need to be patched follow
};

// describe a binary program parameter (CgParameter is opaque)
struct CgBinaryParameter
{
	CGtype type;                                  // cgGetParameterType()
	CGresource res;                               // cgGetParameterResource()
	CGenum var;                                   // cgGetParameterVariability()
	CGint resIndex;                               // cgGetParameterResourceIndex()
	CgBinaryStringOffset name;                    // cgGetParameterName()
	CgBinaryFloatOffset defaultValue;             // default constant value
	CgBinaryEmbeddedConstantOffset embeddedConst; // embedded constant information
	CgBinaryStringOffset semantic;                // cgGetParameterSemantic()
	CGenum direction;                             // cgGetParameterDirection()
	CGint paramno;                                // 0..n: cgGetParameterIndex() -1: globals
	CGbool isReferenced;                          // cgIsParameterReferenced()
	CGbool isShared;                              // cgIsParameterShared()
};

// attributes needed for vshaders
struct CgBinaryVertexProgram
{
	CgBinarySize instructionCount;  // #instructions
	CgBinarySize instructionSlot;   // load address (indexed reads!)
	CgBinarySize registerCount;     // R registers count
	CGbitfield attributeInputMask;  // attributes vs reads from
	CGbitfield attributeOutputMask; // attributes vs writes (uses SET_VERTEX_ATTRIB_OUTPUT_MASK bits)
	CGbitfield userClipMask;        // user clip plane enables (for SET_USER_CLIP_PLANE_CONTROL)
};

enum CgBinaryPartialTexType
{
	CgBinaryPTTNone = 0,
	CgBinaryPTT2x16 = 1,
	CgBinaryPTT1x32 = 2
};

// attributes needed for pshaders
struct CgBinaryFragmentProgram
{
	CgBinarySize instructionCount;   // #instructions
	CGbitfield attributeInputMask;   // attributes fp reads (uses SET_VERTEX_ATTRIB_OUTPUT_MASK bits)
	CGbitfield partialTexType;       // texid 0..15 use two bits each marking whether the texture format requires partial load: see CgBinaryPartialTexType
	CGbitfield16 texCoordsInputMask; // tex coords used by frag prog. (tex<n> is bit n)
	CGbitfield16 texCoords2D;        // tex coords that are 2d        (tex<n> is bit n)
	CGbitfield16 texCoordsCentroid;  // tex coords that are centroid  (tex<n> is bit n)
	u8 registerCount;                // R registers count
	u8 outputFromH0;                 // final color from R0 or H0
	u8 depthReplace;                 // fp generated z depth value
	u8 pixelKill;                    // fp uses kill operations
};

struct CgBinaryProgram
{
	// vertex/pixel shader identification (BE/LE as well)
	CGprofile profile;

	// binary revision (used to verify binary and driver structs match)
	CgBinarySize binaryFormatRevision;

	// total size of this struct including profile and totalSize field
	CgBinarySize totalSize;

	// parameter usually queried using cgGet[First/Next]LeafParameter
	CgBinarySize parameterCount;
	CgBinaryParameterOffset parameterArray;

	// depending on profile points to a CgBinaryVertexProgram or CgBinaryFragmentProgram struct
	CgBinaryOffset program;

	// raw ucode data
	CgBinarySize ucodeSize;
	CgBinaryOffset ucode;

	// variable length data follows
	u8 data[1];
};

inline std::size_t validateCgBinaryProgram(std::span<const std::byte> data)
{
	if (data.size() <= sizeof(CgBinaryProgram))
	{
		return 0;
	}

	CgBinaryProgram header;
	std::memcpy(&header, data.data(), sizeof(header));

	if (header.profile != 7004u && header.profile != 7003u)
	{
		return 0;
	}

	auto totalSize = header.totalSize.value();
	if (sizeof(header) >= totalSize)
	{
		return 0;
	}

	if (header.ucode + header.ucodeSize > totalSize)
	{
		return 0;
	}

	if (header.program >= totalSize)
	{
		return 0;
	}

	if (header.profile == 7004u)
	{
		if (header.program + sizeof(CgBinaryFragmentProgram) > totalSize)
		{
			return 0;
		}
	}
	else
	{
		if (header.program + sizeof(CgBinaryVertexProgram) > totalSize)
		{
			return 0;
		}
	}

	return totalSize;
}

class CgBinaryDisasm
{
	OPDEST dst;
	SRC0 src0;
	SRC1 src1;
	SRC2 src2;

	D0 d0;
	D1 d1;
	D2 d2;
	D3 d3;
	SRC src[3];

	u8* m_buffer = nullptr;
	usz m_buffer_size = 0;
	std::string m_arb_shader;
	std::string m_glsl_shader;
	std::string m_dst_reg_name;

	// FP members
	u32 m_offset = 0;
	u32 m_opcode = 0;
	u32 m_step = 0;
	u32 m_size = 0;
	std::vector<u32> m_end_offsets;
	std::vector<u32> m_else_offsets;
	std::vector<u32> m_loop_end_offsets;

	// VP members
	u32 m_sca_opcode;
	u32 m_vec_opcode;
	static const usz m_max_instr_count = 512;
	usz m_instr_count;
	std::vector<u32> m_data;

public:
	std::string GetArbShader() const
	{
		return m_arb_shader;
	}
	std::string GetGlslShader() const
	{
		return m_glsl_shader;
	}

	// FP functions
	std::string GetMask() const;
	void AddCodeAsm(const std::string& code);
	std::string AddRegDisAsm(u32 index, int fp16) const;
	std::string AddConstDisAsm();
	std::string AddTexDisAsm() const;
	std::string FormatDisAsm(const std::string& code);
	std::string GetCondDisAsm() const;
	template <typename T>
	std::string GetSrcDisAsm(T src);

	// VP functions
	std::string GetMaskDisasm(bool is_sca) const;
	std::string GetVecMaskDisasm() const;
	std::string GetScaMaskDisasm() const;
	std::string GetDSTDisasm(bool is_sca = false) const;
	std::string GetSRCDisasm(u32 n) const;
	static std::string GetTexDisasm();
	std::string GetCondDisasm() const;
	std::string AddAddrMaskDisasm() const;
	std::string AddAddrRegDisasm() const;
	u32 GetAddrDisasm() const;
	std::string FormatDisasm(const std::string& code) const;
	void AddScaCodeDisasm(const std::string& code = "");
	void AddVecCodeDisasm(const std::string& code = "");
	void AddCodeCondDisasm(const std::string& dst, const std::string& src);
	void AddCodeDisasm(const std::string& code);
	void SetDSTDisasm(bool is_sca, const std::string& value);
	void SetDSTVecDisasm(const std::string& code);
	void SetDSTScaDisasm(const std::string& code);

	CgBinaryDisasm(const std::string& path)
	{
		fs::file f(path);
		if (!f)
			return;

		m_buffer_size = f.size();
		m_buffer = new u8[m_buffer_size];
		f.read(m_buffer, m_buffer_size);
		fmt::append(m_arb_shader, "Loading... [%s]\n", path.c_str());
	}

	~CgBinaryDisasm()
	{
		delete[] m_buffer;
	}

	static std::string GetCgParamType(u32 type)
	{
		switch (type)
		{
		case 1045: return "float";
		case 1046:
		case 1047:
		case 1048: return fmt::format("float%d", type - 1044);
		case 1064: return "float4x4";
		case 1066: return "sampler2D";
		case 1069: return "samplerCUBE";
		case 1091: return "float1";

		default: return fmt::format("!UnkCgType(%d)", type);
		}
	}

	std::string GetCgParamName(u32 offset) const
	{
		return std::string(reinterpret_cast<char*>(&m_buffer[offset]));
	}

	std::string GetCgParamRes(u32 /*offset*/) const
	{
		// rsx_log.warning("GetCgParamRes offset 0x%x", offset);
		// TODO
		return "";
	}

	std::string GetCgParamSemantic(u32 offset) const
	{
		return std::string(reinterpret_cast<char*>(&m_buffer[offset]));
	}

	std::string GetCgParamValue(u32 offset, u32 end_offset) const
	{
		std::string offsets = "offsets:";

		u32 num = 0;
		offset += 6;
		while (offset < end_offset)
		{
			fmt::append(offsets, " %d,", m_buffer[offset] << 8 | m_buffer[offset + 1]);
			offset += 4;
			num++;
		}

		if (num > 4)
			return "";

		offsets.pop_back();
		return fmt::format("num %d ", num) + offsets;
	}

	template <typename T>
	T readData(const u32 offset)
	{
		T result;
		std::memcpy(&result, m_buffer + offset, sizeof(result));
		return result;
	}

	void BuildShaderBody()
	{
		ParamArray param_array;

		auto prog = readData<CgBinaryProgram>(0);

		ensure(prog.profile == 7003u || prog.profile == 7004u);

		if (prog.profile == 7004u)
		{
			auto fprog = readData<CgBinaryFragmentProgram>(prog.program);
			m_arb_shader += "\n";
			fmt::append(m_arb_shader, "# binaryFormatRevision 0x%x\n", prog.binaryFormatRevision);
			fmt::append(m_arb_shader, "# profile sce_fp_rsx\n");
			fmt::append(m_arb_shader, "# parameterCount %d\n", prog.parameterCount);
			fmt::append(m_arb_shader, "# instructionCount %d\n", fprog.instructionCount);
			fmt::append(m_arb_shader, "# attributeInputMask 0x%x\n", fprog.attributeInputMask);
			fmt::append(m_arb_shader, "# registerCount %d\n\n", fprog.registerCount);

			CgBinaryParameterOffset offset = prog.parameterArray;
			for (u32 i = 0; i < prog.parameterCount; i++)
			{
				auto fparam = readData<CgBinaryParameter>(offset);

				std::string param_type = GetCgParamType(fparam.type) + " ";
				std::string param_name = GetCgParamName(fparam.name) + " ";
				std::string param_res = GetCgParamRes(fparam.res) + " ";
				std::string param_semantic = GetCgParamSemantic(fparam.semantic) + " ";
				std::string param_const = GetCgParamValue(fparam.embeddedConst, fparam.name);

				fmt::append(m_arb_shader, "#%d%s%s%s%s\n", i, param_type, param_name, param_semantic, param_const);

				offset += u32{sizeof(CgBinaryParameter)};
			}

			m_arb_shader += "\n";
			m_offset = prog.ucode;
			TaskFP();

			std::vector<u32> be_data;

			// Swap bytes. FP decompiler expects input in BE
			for (u32 *ptr = reinterpret_cast<u32*>(m_buffer + m_offset),
					 *end = reinterpret_cast<u32*>(m_buffer + m_buffer_size);
				ptr < end; ++ptr)
			{
				be_data.push_back(std::bit_cast<be_t<u32>>(*ptr));
			}

			RSXFragmentProgram prog;
			auto metadata = program_hash_util::fragment_program_utils::analyse_fragment_program(be_data.data());
			prog.ctrl = (fprog.outputFromH0 ? 0 : 0x40) | (fprog.depthReplace ? 0xe : 0);
			prog.offset = metadata.program_start_offset;
			prog.ucode_length = metadata.program_ucode_length;
			prog.total_length = metadata.program_ucode_length + metadata.program_start_offset;
			prog.data = reinterpret_cast<u8*>(be_data.data()) + metadata.program_start_offset;
			for (u32 i = 0; i < 16; ++i)
				prog.texture_state.set_dimension(rsx::texture_dimension_extended::texture_dimension_2d, i);
#ifndef WITHOUT_OPENGL
			u32 unused;
			GLFragmentDecompilerThread(m_glsl_shader, param_array, prog, unused).Task();
#endif
		}

		else
		{
			const auto vprog = readData<CgBinaryVertexProgram>(prog.program);
			m_arb_shader += "\n";
			fmt::append(m_arb_shader, "# binaryFormatRevision 0x%x\n", prog.binaryFormatRevision);
			fmt::append(m_arb_shader, "# profile sce_vp_rsx\n");
			fmt::append(m_arb_shader, "# parameterCount %d\n", prog.parameterCount);
			fmt::append(m_arb_shader, "# instructionCount %d\n", vprog.instructionCount);
			fmt::append(m_arb_shader, "# registerCount %d\n", vprog.registerCount);
			fmt::append(m_arb_shader, "# attributeInputMask 0x%x\n", vprog.attributeInputMask);
			fmt::append(m_arb_shader, "# attributeOutputMask 0x%x\n\n", vprog.attributeOutputMask);

			CgBinaryParameterOffset offset = prog.parameterArray;
			for (u32 i = 0; i < prog.parameterCount; i++)
			{
				auto vparam = readData<CgBinaryParameter>(offset);

				std::string param_type = GetCgParamType(vparam.type) + " ";
				std::string param_name = GetCgParamName(vparam.name.get()) + " ";
				std::string param_res = GetCgParamRes(vparam.res) + " ";
				std::string param_semantic = GetCgParamSemantic(vparam.semantic) + " ";
				std::string param_const = GetCgParamValue(vparam.embeddedConst, vparam.name);

				fmt::append(m_arb_shader, "#%d%s%s%s%s\n", i, param_type, param_name, param_semantic, param_const);

				offset += u32{sizeof(CgBinaryParameter)};
			}

			m_arb_shader += "\n";
			m_offset = prog.ucode;
			ensure((m_buffer_size - m_offset) % sizeof(u32) == 0);

			auto vdata = reinterpret_cast<be_t<u32>*>(&m_buffer[m_offset]);
			m_data.resize(prog.ucodeSize / sizeof(u32));
			for (std::size_t i = 0; i < m_data.size(); ++i)
			{
				m_data[i] = vdata[i];
			}
			TaskVP();

			RSXVertexProgram prog;
			program_hash_util::vertex_program_utils::analyse_vertex_program(m_data.data(), 0, prog);
			for (u32 i = 0; i < 4; ++i)
				prog.texture_state.set_dimension(rsx::texture_dimension_extended::texture_dimension_2d, i);
#ifndef WITHOUT_OPENGL
			GLVertexDecompilerThread(prog, m_glsl_shader, param_array).Task();
#endif
		}
	}

	static u32 GetData(const u32 d)
	{
		return d << 16 | d >> 16;
	}
	void TaskFP();
	void TaskVP();
};
