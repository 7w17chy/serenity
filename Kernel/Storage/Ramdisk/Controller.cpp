/*
 * Copyright (c) 2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/OwnPtr.h>
#include <AK/Types.h>
#include <Kernel/Library/LockRefPtr.h>
#include <Kernel/Storage/Ramdisk/Controller.h>

namespace Kernel {

ErrorOr<NonnullRefPtr<RamdiskController>> RamdiskController::try_initialize()
{
    return TRY(adopt_nonnull_ref_or_enomem(new (nothrow) RamdiskController()));
}

ErrorOr<void> RamdiskController::reset()
{
    return Error::from_errno(ENOTIMPL);
}

ErrorOr<void> RamdiskController::shutdown()
{
    return Error::from_errno(ENOTIMPL);
}

size_t RamdiskController::devices_count() const
{
    return m_devices.size();
}

void RamdiskController::complete_current_request(AsyncDeviceRequest::RequestResult)
{
    VERIFY_NOT_REACHED();
}

RamdiskController::RamdiskController()
    : StorageController(0)
{
    // Populate ramdisk controllers from Multiboot boot modules, if any.
    size_t count = 0;
    MM.for_each_used_memory_range([&](auto& used_memory_range) {
        if (used_memory_range.type == Memory::UsedMemoryRangeType::BootModule) {
            size_t length = Memory::page_round_up(used_memory_range.end.get()).release_value_but_fixme_should_propagate_errors() - used_memory_range.start.get();
            auto region_or_error = MM.allocate_kernel_region(used_memory_range.start, length, "Ramdisk"sv, Memory::Region::Access::ReadWrite);
            if (region_or_error.is_error()) {
                dmesgln("RamdiskController: Failed to allocate kernel region of size {}", length);
            } else {
                m_devices.append(RamdiskDevice::create(*this, region_or_error.release_value(), 6, count));
            }
            count++;
        }
    });
}

RamdiskController::~RamdiskController() = default;

LockRefPtr<StorageDevice> RamdiskController::device(u32 index) const
{
    if (index >= m_devices.size())
        return nullptr;
    return m_devices[index];
}

}
