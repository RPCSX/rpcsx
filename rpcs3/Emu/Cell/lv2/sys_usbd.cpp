#include "stdafx.h"
#include "sys_usbd.h"
#include "sys_ppu_thread.h"
#include "sys_sync.h"

#include "Emu/Cell/PPUThread.h"
#include "Emu/Cell/ErrorCodes.h"

LOG_CHANNEL(sys_usbd);

struct UsbLdd
{
	u16 id_vendor{};
	u16 id_product_min{};
	u16 id_product_max{};
};


error_code sys_usbd_initialize(ppu_thread& ppu, vm::ptr<u32> handle)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_initialize(handle=*0x%x)", handle);

	ppu.check_state();
	*handle = 0x115B;

	// TODO
	return CELL_OK;
}

error_code sys_usbd_finalize(ppu_thread& ppu, u32 handle)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_finalize(handle=0x%x)", handle);

	// TODO
	return CELL_OK;
}

error_code sys_usbd_get_device_list(ppu_thread& ppu, u32 handle, vm::ptr<UsbInternalDevice> device_list, u32 max_devices)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_get_device_list(handle=0x%x, device_list=*0x%x, max_devices=0x%x)", handle, device_list, max_devices);

	return CELL_OK;
}

error_code sys_usbd_register_extra_ldd(ppu_thread& ppu, u32 handle, vm::cptr<char> s_product, u16 slen_product, u16 id_vendor, u16 id_product_min, u16 id_product_max)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_register_extra_ldd(handle=0x%x, s_product=%s, slen_product=%d, id_vendor=0x%04x, id_product_min=0x%04x, id_product_max=0x%04x)", handle, s_product, slen_product, id_vendor, id_product_min, id_product_max);

	return CELL_OK;
}

error_code sys_usbd_unregister_extra_ldd(ppu_thread& ppu, u32 handle, vm::cptr<char> s_product, u16 slen_product)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_unregister_extra_ldd(handle=0x%x, s_product=%s, slen_product=%d)", handle, s_product, slen_product);

	return CELL_OK;
}

error_code sys_usbd_get_descriptor_size(ppu_thread& ppu, u32 handle, u32 device_handle)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_get_descriptor_size(handle=0x%x, deviceNumber=0x%x)", handle, device_handle);

	return CELL_OK;
}

error_code sys_usbd_get_descriptor(ppu_thread& ppu, u32 handle, u32 device_handle, vm::ptr<void> descriptor, u32 desc_size)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_get_descriptor(handle=0x%x, deviceNumber=0x%x, descriptor=0x%x, desc_size=0x%x)", handle, device_handle, descriptor, desc_size);

	if (!descriptor)
	{
		return CELL_EINVAL;
	}

	return CELL_OK;
}

// This function is used for psp(cellUsbPspcm), ps3 arcade usj io(PS3A-USJ), ps2 cam(eyetoy), generic usb camera?(sample_usb2cam)
error_code sys_usbd_register_ldd(ppu_thread& ppu, u32 handle, vm::cptr<char> s_product, u16 slen_product)
{
	ppu.state += cpu_flag::wait;

	std::string_view product{s_product.get_ptr(), slen_product};

	// slightly hacky way of getting Namco GCon3 gun to work.
	// The register_ldd appears to be a more promiscuous mode function, where all device 'inserts' would be presented to the cellUsbd for Probing.
	// Unsure how many more devices might need similar treatment (i.e. just a compare and force VID/PID add), or if it's worth adding a full promiscuous capability
	static const std::unordered_map<std::string, UsbLdd, fmt::string_hash, std::equal_to<>> predefined_ldds{
		{"cellUsbPspcm", {0x054C, 0x01CB, 0x01CB}},
		{"guncon3", {0x0B9A, 0x0800, 0x0800}},
		{"PS3A-USJ", {0x0B9A, 0x0900, 0x0910}}};

	if (const auto iterator = predefined_ldds.find(product); iterator != predefined_ldds.end())
	{
		sys_usbd.trace("sys_usbd_register_ldd(handle=0x%x, s_product=%s, slen_product=%d) -> Redirecting to sys_usbd_register_extra_ldd()", handle, s_product, slen_product);
		return sys_usbd_register_extra_ldd(ppu, handle, s_product, slen_product, iterator->second.id_vendor, iterator->second.id_product_min, iterator->second.id_product_max);
	}

	sys_usbd.todo("sys_usbd_register_ldd(handle=0x%x, s_product=%s, slen_product=%d)", handle, s_product, slen_product);
	return CELL_OK;
}

error_code sys_usbd_unregister_ldd(ppu_thread& ppu, u32 handle, vm::cptr<char> s_product, u16 slen_product)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.trace("sys_usbd_unregister_ldd(handle=0x%x, s_product=%s, slen_product=%d) -> Redirecting to sys_usbd_unregister_extra_ldd()", handle, s_product, slen_product);

	return sys_usbd_unregister_extra_ldd(ppu, handle, s_product, slen_product);
}

// TODO: determine what the unknown params are
// attributes (bmAttributes) : 2=Bulk, 3=Interrupt
error_code sys_usbd_open_pipe(ppu_thread& ppu, u32 handle, u32 device_handle, u32 unk1, u64 unk2, u64 unk3, u32 endpoint, u64 attributes)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_open_pipe(handle=0x%x, device_handle=0x%x, unk1=0x%x, unk2=0x%x, unk3=0x%x, endpoint=0x%x, attributes=0x%x)", handle, device_handle, unk1, unk2, unk3, endpoint, attributes);

	return CELL_OK;
}

error_code sys_usbd_open_default_pipe(ppu_thread& ppu, u32 handle, u32 device_handle)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_open_default_pipe(handle=0x%x, device_handle=0x%x)", handle, device_handle);

	return CELL_OK;
}

error_code sys_usbd_close_pipe(ppu_thread& ppu, u32 handle, u32 pipe_handle)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_close_pipe(handle=0x%x, pipe_handle=0x%x)", handle, pipe_handle);

	return CELL_OK;
}

// From RE:
// In libusbd_callback_thread
// *arg1 = 4 will terminate CellUsbd libusbd_callback_thread
// *arg1 = 3 will do some extra processing right away(notification of transfer finishing)
// *arg1 < 1 || *arg1 > 4 are ignored(rewait instantly for event)
// *arg1 == 1 || *arg1 == 2 will send a sys_event to internal CellUsbd event queue with same parameters as received and loop(attach and detach event)
error_code sys_usbd_receive_event(ppu_thread& ppu, u32 handle, vm::ptr<u64> arg1, vm::ptr<u64> arg2, vm::ptr<u64> arg3)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_receive_event(handle=0x%x, arg1=*0x%x, arg2=*0x%x, arg3=*0x%x)", handle, arg1, arg2, arg3);

	ppu.check_state();
	*arg1 = ppu.gpr[4];
	*arg2 = ppu.gpr[5];
	*arg3 = ppu.gpr[6];

	if (*arg1 == SYS_USBD_ATTACH)
		lv2_obj::sleep(ppu), lv2_obj::wait_timeout(5000);

	return CELL_OK;
}

error_code sys_usbd_detect_event(ppu_thread& ppu)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_detect_event()");
	return CELL_OK;
}

error_code sys_usbd_attach(ppu_thread& ppu, u32 handle, u32 unk1, u32 unk2, u32 device_handle)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_attach(handle=0x%x, unk1=0x%x, unk2=0x%x, device_handle=0x%x)", handle, unk1, unk2, device_handle);
	return CELL_OK;
}

error_code sys_usbd_transfer_data(ppu_thread& ppu, u32 handle, u32 id_pipe, vm::ptr<u8> buf, u32 buf_size, vm::ptr<UsbDeviceRequest> request, u32 type_transfer)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_transfer_data(handle=0x%x, id_pipe=0x%x, buf=*0x%x, buf_length=0x%x, request=*0x%x, type=0x%x)", handle, id_pipe, buf, buf_size, request, type_transfer);

	if (sys_usbd.trace && request)
	{
		sys_usbd.trace("RequestType:0x%02x, Request:0x%02x, wValue:0x%04x, wIndex:0x%04x, wLength:0x%04x", request->bmRequestType, request->bRequest, request->wValue, request->wIndex, request->wLength);

		if ((request->bmRequestType & 0x80) == 0 && buf && buf_size != 0)
			sys_usbd.trace("Control sent:\n%s", fmt::buf_to_hexstring(buf.get_ptr(), buf_size));
	}

	// returns an identifier specific to the transfer
	return CELL_OK;
}

error_code sys_usbd_isochronous_transfer_data(ppu_thread& ppu, u32 handle, u32 id_pipe, vm::ptr<UsbDeviceIsoRequest> iso_request)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_isochronous_transfer_data(handle=0x%x, id_pipe=0x%x, iso_request=*0x%x)", handle, id_pipe, iso_request);

	// returns an identifier specific to the transfer
	return CELL_OK;
}

error_code sys_usbd_get_transfer_status(ppu_thread& ppu, u32 handle, u32 id_transfer, u32 unk1, vm::ptr<u32> result, vm::ptr<u32> count)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_get_transfer_status(handle=0x%x, id_transfer=0x%x, unk1=0x%x, result=*0x%x, count=*0x%x)", handle, id_transfer, unk1, result, count);

	*result = 0;
	*count = 0;

	return CELL_OK;
}

error_code sys_usbd_get_isochronous_transfer_status(ppu_thread& ppu, u32 handle, u32 id_transfer, u32 unk1, vm::ptr<UsbDeviceIsoRequest> request, vm::ptr<u32> result)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_get_isochronous_transfer_status(handle=0x%x, id_transfer=0x%x, unk1=0x%x, request=*0x%x, result=*0x%x)", handle, id_transfer, unk1, request, result);

	*result = 0;

	return CELL_OK;
}

error_code sys_usbd_get_device_location(ppu_thread& ppu, u32 handle, u32 device_handle, vm::ptr<u8> location)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_get_device_location(handle=0x%x, device_handle=0x%x, location=*0x%x)", handle, device_handle, location);

	return CELL_OK;
}

error_code sys_usbd_send_event(ppu_thread& ppu)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_send_event()");
	return CELL_OK;
}

error_code sys_usbd_event_port_send(ppu_thread& ppu, u32 handle, u64 arg1, u64 arg2, u64 arg3)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_event_port_send(handle=0x%x, arg1=0x%x, arg2=0x%x, arg3=0x%x)", handle, arg1, arg2, arg3);

	return CELL_OK;
}

error_code sys_usbd_allocate_memory(ppu_thread& ppu)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_allocate_memory()");
	return CELL_OK;
}

error_code sys_usbd_free_memory(ppu_thread& ppu)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_free_memory()");
	return CELL_OK;
}

error_code sys_usbd_get_device_speed(ppu_thread& ppu)
{
	ppu.state += cpu_flag::wait;

	sys_usbd.todo("sys_usbd_get_device_speed()");
	return CELL_OK;
}

void connect_usb_controller(u8 index, input::product_type)
{
}

void handle_hotplug_event(bool connected)
{
}
