#pragma once
#include "../system_config.h"
#include "util/File.h"
#include "util/lockless.h"
#include "util/Thread.h"
#include "Common/bitfield.hpp"
#include "Common/unordered_map.hpp"
#include "Emu/System.h"
#include "Emu/cache_utils.hpp"
#include "Emu/RSX/Program/RSXVertexProgram.h"
#include "Emu/RSX/Program/RSXFragmentProgram.h"
#include "Overlays/Shaders/shader_loading_dialog.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "util/sysinfo.hpp"
#include "util/fnv_hash.hpp"

namespace rsx
{
	template <typename pipeline_storage_type, typename backend_storage>
	class shaders_cache
	{
		struct unpacked_shader
		{
			pipeline_storage_type props;
			RSXVertexProgram vp;
			RSXFragmentProgram fp;
		};

		using unpacked_type = lf_fifo<unpacked_shader, 500>;

		struct pipeline_data
		{
			u64 vertex_program_hash;
			u64 fragment_program_hash;
			u64 pipeline_storage_hash;

			u32 vp_ctrl0;
			u32 vp_ctrl1;
			u32 vp_texture_dimensions;
			u32 vp_reserved_0;
			u64 vp_instruction_mask[9];

			u32 vp_base_address;
			u32 vp_entry;
			u16 vp_jump_table[32];

			u16 vp_multisampled_textures;
			u16 vp_reserved_1;
			u32 vp_reserved_2;

			u32 fp_ctrl;
			u32 fp_texture_dimensions;
			u32 fp_texcoord_control;
			u16 fp_height;
			u16 fp_pixel_layout;
			u16 fp_lighting_flags;
			u16 fp_shadow_textures;
			u16 fp_redirected_textures;
			u16 fp_multisampled_textures;
			u8 fp_mrt_count;
			u8 fp_reserved0;
			u16 fp_reserved1;
			u32 fp_reserved2;

			pipeline_storage_type pipeline_properties;
		};

		std::string version_prefix;
		std::string root_path;
		std::string pipeline_class_name;
		lf_fifo<std::unique_ptr<u8[]>, 100> fragment_program_data;

		backend_storage& m_storage;

		std::atomic<bool> m_shader_storage_exit{false};
		std::condition_variable m_shader_storage_cv;
		std::mutex m_shader_storage_mtx;
		std::vector<unpacked_shader> m_shader_storage_worker_queue;

		std::thread m_shader_storage_worker_thread = std::thread([this]
			{
				while (!m_shader_storage_exit.load())
				{
					unpacked_shader item;

					{
						std::unique_lock lock(m_shader_storage_mtx);
						m_shader_storage_cv.wait(lock);
						if (m_shader_storage_worker_queue.empty())
						{
							continue;
						}

						item = std::move(m_shader_storage_worker_queue.back());
						m_shader_storage_worker_queue.pop_back();
					}

					pipeline_data data = pack(item.props, item.vp, item.fp);

					std::string fp_name = root_path + "/raw/" + fmt::format("%llX.fp", data.fragment_program_hash);
					std::string vp_name = root_path + "/raw/" + fmt::format("%llX.vp", data.vertex_program_hash);

					if (fs::stat_t s{}; !fs::get_stat(fp_name, s) || s.size != item.fp.ucode_length)
					{
						fs::write_pending_file(fp_name, item.fp.get_data(), item.fp.ucode_length);
					}

					if (fs::stat_t s{}; !fs::get_stat(vp_name, s) || s.size != item.vp.data.size() * sizeof(u32))
					{
						fs::write_pending_file(vp_name, item.vp.data);
					}

					const u32 state_params[] =
						{
							data.vp_ctrl0,
							data.vp_ctrl1,
							data.fp_ctrl,
							data.vp_texture_dimensions,
							data.fp_texture_dimensions,
							data.fp_texcoord_control,
							data.fp_height,
							data.fp_pixel_layout,
							data.fp_lighting_flags,
							data.fp_shadow_textures,
							data.fp_redirected_textures,
							data.vp_multisampled_textures,
							data.fp_multisampled_textures,
							data.fp_mrt_count,
					};
					const usz state_hash = rpcs3::hash_array(state_params);

					const std::string pipeline_file_name = fmt::format("%llX+%llX+%llX+%llX.bin", data.vertex_program_hash, data.fragment_program_hash, data.pipeline_storage_hash, state_hash);
					const std::string pipeline_path = root_path + "/pipelines/" + pipeline_class_name + "/" + version_prefix + "/" + pipeline_file_name;
					fs::write_pending_file(pipeline_path, &data, sizeof(data));
				}
			});

		static std::string get_message(u32 index, u32 processed, u32 entry_count)
		{
			return fmt::format("%s pipeline object %u of %u", index == 0 ? "Loading" : "Compiling", processed, entry_count);
		}

		void load_shaders(uint nb_workers, unpacked_type& unpacked, std::string& directory_path, std::vector<fs::dir_entry>& entries, u32 entry_count,
			shader_loading_dialog* dlg)
		{
			atomic_t<u32> processed(0);

			std::function<void(u32, u32)> shader_load_worker = [&](u32 start_at, u32 stop_at)
			{
				u32 thread_processed = 0;
				auto update_stats = [&]
				{
					if (thread_processed == 0)
					{
						return true;
					}

					processed += thread_processed;
					thread_processed = 0;
					return !Emu.IsStopped();
				};

				for (u32 pos = start_at; pos < stop_at; ++pos)
				{
					const fs::dir_entry& tmp = entries[pos];
					thread_processed++;

					const auto filename = directory_path + "/" + tmp.name;
					fs::file f(filename);

					if (!f)
					{
						fs::remove_file(filename);
						continue;
					}

					if (f.size() != sizeof(pipeline_data))
					{
						rsx_log.error("Removing cached pipeline object %s since it's not binary compatible with the current shader cache", tmp.name.c_str());
						fs::remove_file(filename);
						continue;
					}

					pipeline_data pdata{};
					f.read(&pdata, f.size());

					auto entry = unpack(pdata);

					if (entry.vp.data.empty() || !entry.fp.ucode_length)
					{
						continue;
					}

					m_storage.preload_programs(nullptr, entry.vp, entry.fp);

					unpacked[unpacked.push_begin()] = std::move(entry);

					if (thread_processed >= 10 && !update_stats())
					{
						return;
					}
				}

				update_stats();
			};

			await_workers(nb_workers, 0, shader_load_worker, processed, entry_count, dlg);
		}

		template <typename... Args>
		void compile_shaders(uint nb_workers, unpacked_type& unpacked, u32 entry_count, shader_loading_dialog* dlg, Args&&... args)
		{
			atomic_t<u32> processed(0);

			std::function<void(u32, u32)> shader_comp_worker = [&](u32 start_at, u32 stop_at)
			{
				u32 thread_processed = 0;
				auto update_stats = [&]
				{
					if (thread_processed == 0)
					{
						return true;
					}

					processed += thread_processed;
					thread_processed = 0;
					return !Emu.IsStopped();
				};

				for (u32 pos = start_at; pos < stop_at; ++pos)
				{
					unpacked_shader& entry = unpacked[pos];
					m_storage.add_pipeline_entry(entry.vp, entry.fp, entry.props, std::forward<Args>(args)...);
					thread_processed++;

					if (thread_processed >= 3 && !update_stats())
					{
						return;
					}
				}

				update_stats();
			};

			await_workers(nb_workers, 1, shader_comp_worker, processed, entry_count, dlg);
		}

		void await_workers(uint nb_workers, u8 step, std::function<void(u32, u32)>& worker, atomic_t<u32>& processed, u32 entry_count, shader_loading_dialog* dlg)
		{
			if (nb_workers > entry_count)
			{
				nb_workers = entry_count;
			}

			if (nb_workers == 1)
			{
				steady_clock::time_point last_update;

				// Call the worker function directly, stopping it prematurely to be able update the screen
				u32 stop_at = 0;
				u32 start_at = 0;
				do
				{
					stop_at = std::min(start_at + 10, entry_count);

					worker(start_at, stop_at);

					start_at = stop_at;

					// Only update the screen at about 60fps since updating it everytime slows down the process
					steady_clock::time_point now = steady_clock::now();
					if ((std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update) > 16ms) || (stop_at == entry_count))
					{
						dlg->update_msg(step, get_message(step, stop_at, entry_count));
						dlg->set_value(step, stop_at);
						last_update = now;
					}
				} while (stop_at < entry_count && !Emu.IsStopped());
			}
			else
			{
				named_thread_group workers("RSX Worker ", nb_workers, [&](u32 thread_index)
					{
						if (nb_workers == entry_count)
						{
							worker(thread_index, thread_index + 1);
							return;
						}

						auto per_thread_entries = entry_count / nb_workers;
						auto start_at = per_thread_entries * thread_index;
						auto stop_at = thread_index == nb_workers - 1 ? entry_count : start_at + per_thread_entries;
						worker(start_at, stop_at);
					});

				u32 current_progress = 0;
				u32 last_update_progress = 0;
				while ((current_progress < entry_count) && !Emu.IsStopped())
				{
					thread_ctrl::wait_for(16'000); // Around 60fps should be good enough

					if (Emu.IsStopped())
						break;

					current_progress = std::min(processed.load(), entry_count);

					if (last_update_progress != current_progress)
					{
						last_update_progress = current_progress;
						dlg->update_msg(step, get_message(step, current_progress, entry_count));
						dlg->set_value(step, current_progress);
					}
				}
			}

			if (!Emu.IsStopped())
			{
				ensure(processed == entry_count);
			}
		}

	public:
		shaders_cache(backend_storage& storage, std::string pipeline_class, std::string version_prefix_str = "v1")
			: version_prefix(std::move(version_prefix_str)), pipeline_class_name(std::move(pipeline_class)), m_storage(storage)
		{
			if (!g_cfg.video.disable_on_disk_shader_cache)
			{
				if (std::string cache_path = rpcs3::cache::get_ppu_cache(); !cache_path.empty())
				{
					root_path = std::move(cache_path) + "shaders_cache/";
				}
			}
		}

		~shaders_cache()
		{
			{
				std::lock_guard lock(m_shader_storage_mtx);
				m_shader_storage_exit = true;
				m_shader_storage_cv.notify_one();
			}

			m_shader_storage_worker_thread.join();
		}

		template <typename... Args>
		void load(shader_loading_dialog* dlg, Args&&... args)
		{
			if (root_path.empty())
			{
				return;
			}

			std::string directory_path = root_path + "/pipelines/" + pipeline_class_name + "/" + version_prefix;

			fs::dir root = fs::dir(directory_path);

			if (!root)
			{
				fs::create_path(directory_path);
				fs::create_path(root_path + "/raw");
				return;
			}

			std::vector<fs::dir_entry> entries;

			for (auto&& entry : root)
			{
				if (entry.is_directory)
					continue;

				if (entry.name.ends_with(".bin"))
				{
					entries.push_back(std::move(entry));
				}
			}

			u32 entry_count = ::size32(entries);

			if (!entry_count)
				return;

			root.rewind();

			// Progress dialog
			std::unique_ptr<shader_loading_dialog> fallback_dlg;
			if (!dlg)
			{
				fallback_dlg = std::make_unique<shader_loading_dialog>();
				dlg = fallback_dlg.get();
			}

			dlg->create("Preloading cached shaders from disk.\nPlease wait...", "Shader Compilation");
			dlg->set_limit(0, entry_count);
			dlg->set_limit(1, entry_count);
			dlg->update_msg(0, get_message(0, 0, entry_count));
			dlg->update_msg(1, get_message(1, 0, entry_count));

			// Preload everything needed to compile the shaders
			unpacked_type unpacked;
			uint nb_workers = g_cfg.video.renderer == video_renderer::vulkan ? utils::get_thread_count() * 2 : 1;

			load_shaders(nb_workers, unpacked, directory_path, entries, entry_count, dlg);

			// Account for any invalid entries
			entry_count = unpacked.size();

			compile_shaders(nb_workers, unpacked, entry_count, dlg, std::forward<Args>(args)...);

			dlg->refresh();
			dlg->close();
		}

		void store(const pipeline_storage_type& pipeline, const RSXVertexProgram& vp, const RSXFragmentProgram& fp)
		{
			if (root_path.empty())
			{
				return;
			}

			if (vp.jump_table.size() > 32)
			{
				rsx_log.error("shaders_cache: vertex program has more than 32 jump addresses. Entry not saved to cache");
				return;
			}

			auto item = unpacked_shader{pipeline, vp, RSXFragmentProgram::clone(fp) /* ???? */};

			std::lock_guard lock(m_shader_storage_mtx);
			m_shader_storage_worker_queue.push_back(std::move(item));
			m_shader_storage_cv.notify_one();
		}

		void wait_stores()
		{
			while (true)
			{
				{
					std::lock_guard lock(m_shader_storage_mtx);
					if (m_shader_storage_worker_queue.empty())
					{
						return;
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}

		RSXVertexProgram load_vp_raw(u64 program_hash) const
		{
			RSXVertexProgram vp = {};

			fs::file f(fmt::format("%s/raw/%llX.vp", root_path, program_hash));
			if (f)
				f.read(vp.data, f.size() / sizeof(u32));

			return vp;
		}

		RSXFragmentProgram load_fp_raw(u64 program_hash)
		{
			fs::file f(fmt::format("%s/raw/%llX.fp", root_path, program_hash));

			RSXFragmentProgram fp = {};

			const u32 size = fp.ucode_length = f ? ::size32(f) : 0;

			if (!size)
			{
				return fp;
			}

			auto buf = std::make_unique<u8[]>(size);
			fp.data = buf.get();
			f.read(buf.get(), size);
			fragment_program_data[fragment_program_data.push_begin()] = std::move(buf);
			return fp;
		}

		unpacked_shader unpack(pipeline_data& data)
		{
			unpacked_shader result;
			result.vp = load_vp_raw(data.vertex_program_hash);
			result.fp = load_fp_raw(data.fragment_program_hash);
			result.props = data.pipeline_properties;

			result.vp.ctrl = data.vp_ctrl0;
			result.vp.output_mask = data.vp_ctrl1;
			result.vp.texture_state.texture_dimensions = data.vp_texture_dimensions;
			result.vp.texture_state.multisampled_textures = data.vp_multisampled_textures;
			result.vp.base_address = data.vp_base_address;
			result.vp.entry = data.vp_entry;

			pack_bitset<max_vertex_program_instructions>(result.vp.instruction_mask, data.vp_instruction_mask);

			for (u8 index = 0; index < 32; ++index)
			{
				const auto address = data.vp_jump_table[index];
				if (address == u16{umax})
				{
					// End of list marker
					break;
				}

				result.vp.jump_table.emplace(address);
			}

			result.fp.ctrl = data.fp_ctrl;
			result.fp.texture_state.texture_dimensions = data.fp_texture_dimensions;
			result.fp.texture_state.shadow_textures = data.fp_shadow_textures;
			result.fp.texture_state.redirected_textures = data.fp_redirected_textures;
			result.fp.texture_state.multisampled_textures = data.fp_multisampled_textures;
			result.fp.texcoord_control_mask = data.fp_texcoord_control;
			result.fp.two_sided_lighting = !!(data.fp_lighting_flags & 0x1);
			result.fp.mrt_buffers_count = data.fp_mrt_count;

			return result;
		}

		pipeline_data pack(const pipeline_storage_type& pipeline, const RSXVertexProgram& vp, const RSXFragmentProgram& fp)
		{
			pipeline_data data_block = {};
			data_block.pipeline_properties = pipeline;
			data_block.vertex_program_hash = m_storage.get_hash(vp);
			data_block.fragment_program_hash = m_storage.get_hash(fp);
			data_block.pipeline_storage_hash = m_storage.get_hash(pipeline);

			data_block.vp_ctrl0 = vp.ctrl;
			data_block.vp_ctrl1 = vp.output_mask;
			data_block.vp_texture_dimensions = vp.texture_state.texture_dimensions;
			data_block.vp_multisampled_textures = vp.texture_state.multisampled_textures;
			data_block.vp_base_address = vp.base_address;
			data_block.vp_entry = vp.entry;

			unpack_bitset<max_vertex_program_instructions>(vp.instruction_mask, data_block.vp_instruction_mask);

			u8 index = 0;
			while (index < 32)
			{
				if (!index && !vp.jump_table.empty())
				{
					for (auto& address : vp.jump_table)
					{
						data_block.vp_jump_table[index++] = static_cast<u16>(address);
					}
				}
				else
				{
					// End of list marker
					data_block.vp_jump_table[index] = u16{umax};
					break;
				}
			}

			data_block.fp_ctrl = fp.ctrl;
			data_block.fp_texture_dimensions = fp.texture_state.texture_dimensions;
			data_block.fp_texcoord_control = fp.texcoord_control_mask;
			data_block.fp_lighting_flags = u16(fp.two_sided_lighting);
			data_block.fp_shadow_textures = fp.texture_state.shadow_textures;
			data_block.fp_redirected_textures = fp.texture_state.redirected_textures;
			data_block.fp_multisampled_textures = fp.texture_state.multisampled_textures;
			data_block.fp_mrt_count = fp.mrt_buffers_count;

			return data_block;
		}
	};

	namespace vertex_cache
	{
		// A null vertex cache
		template <typename storage_type>
		class default_vertex_cache
		{
		public:
			virtual ~default_vertex_cache() = default;
			virtual const storage_type* find_vertex_range(u32 /*local_addr*/, u32 /*data_length*/)
			{
				return nullptr;
			}
			virtual void store_range(u32 /*local_addr*/, u32 /*data_length*/, u32 /*offset_in_heap*/) {}
			virtual void purge() {}
		};

		struct uploaded_range
		{
			uptr local_address;
			u32 offset_in_heap;
			u32 data_length;
		};

		// A weak vertex cache with no data checks or memory range locks
		// Of limited use since contents are only guaranteed to be valid once per frame
		// Supports upto 1GiB block lengths if typed and full 4GiB otherwise.
		// Using a 1:1 hash-value with robin-hood is 2x faster than what we had before with std-map-of-arrays.
		class weak_vertex_cache : public default_vertex_cache<uploaded_range>
		{
			using storage_type = uploaded_range;

		private:
			rsx::unordered_map<uptr, storage_type> vertex_ranges;

			FORCE_INLINE u64 hash(u32 local_addr, u32 data_length) const
			{
				return u64(local_addr) | (u64(data_length) << 32);
			}

		public:
			const storage_type* find_vertex_range(u32 local_addr, u32 data_length) override
			{
				const auto key = hash(local_addr, data_length);
				const auto found = vertex_ranges.find(key);
				if (found == vertex_ranges.end())
				{
					return nullptr;
				}

				return std::addressof(found->second);
			}

			void store_range(u32 local_addr, u32 data_length, u32 offset_in_heap) override
			{
				storage_type v = {};
				v.data_length = data_length;
				v.local_address = local_addr;
				v.offset_in_heap = offset_in_heap;

				const auto key = hash(local_addr, data_length);
				vertex_ranges[key] = v;
			}

			void purge() override
			{
				vertex_ranges.clear();
			}
		};
	} // namespace vertex_cache
} // namespace rsx
