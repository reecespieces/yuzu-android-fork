// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_transfer_memory.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {
namespace {

constexpr bool IsValidTransferMemoryPermission(MemoryPermission perm) {
    switch (perm) {
    case MemoryPermission::None:
    case MemoryPermission::Read:
    case MemoryPermission::ReadWrite:
        return true;
    default:
        return false;
    }
}

} // Anonymous namespace

/// Creates a TransferMemory object
Result CreateTransferMemory(Core::System& system, Handle* out, VAddr address, u64 size,
                            MemoryPermission map_perm) {
    auto& kernel = system.Kernel();

    // Validate the size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Validate the permissions.
    R_UNLESS(IsValidTransferMemoryPermission(map_perm), ResultInvalidNewMemoryPermission);

    // Get the current process and handle table.
    auto& process = *kernel.CurrentProcess();
    auto& handle_table = process.GetHandleTable();

    // Reserve a new transfer memory from the process resource limit.
    KScopedResourceReservation trmem_reservation(kernel.CurrentProcess(),
                                                 LimitableResource::TransferMemoryCountMax);
    R_UNLESS(trmem_reservation.Succeeded(), ResultLimitReached);

    // Create the transfer memory.
    KTransferMemory* trmem = KTransferMemory::Create(kernel);
    R_UNLESS(trmem != nullptr, ResultOutOfResource);

    // Ensure the only reference is in the handle table when we're done.
    SCOPE_EXIT({ trmem->Close(); });

    // Ensure that the region is in range.
    R_UNLESS(process.PageTable().Contains(address, size), ResultInvalidCurrentMemory);

    // Initialize the transfer memory.
    R_TRY(trmem->Initialize(address, size, map_perm));

    // Commit the reservation.
    trmem_reservation.Commit();

    // Register the transfer memory.
    KTransferMemory::Register(kernel, trmem);

    // Add the transfer memory to the handle table.
    R_TRY(handle_table.Add(out, trmem));

    return ResultSuccess;
}

Result CreateTransferMemory32(Core::System& system, Handle* out, u32 address, u32 size,
                              MemoryPermission map_perm) {
    return CreateTransferMemory(system, out, address, size, map_perm);
}
} // namespace Kernel::Svc
