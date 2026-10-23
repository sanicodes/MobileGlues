// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

#include "../../3rdparty/SPIRV-Cross/spirv_glsl.hpp"

// ESSL requires constant indices for fragment output arrays, even when a loop
// has constant bounds. Keep calculations in a private array and copy to the
// original output with literal indices at every normal return from main.
class ESSLCompiler : public spirv_cross::CompilerGLSL {
public:
    explicit ESSLCompiler(std::vector<uint32_t> spirv) : CompilerGLSL(std::move(spirv)) {
        using namespace spirv_cross;
        if (get_execution_model() != spv::ExecutionModelFragment) return;

        const auto outputs = get_shader_resources().stage_outputs;
        for (const auto& output : outputs) {
            const auto type = get_type(output.type_id);
            if (type.array.empty()) continue;
            // Fragment outputs cannot be arrays of arrays or unsized arrays.
            if (type.array.size() != 1 || !type.array_size_literal[0] || type.array[0] == 0)
                SPIRV_CROSS_THROW("Unsupported fragment output array size.");

            const uint32_t ids = ir.increase_bound_by(2);
            auto& private_type = set<SPIRType>(ids, type);
            private_type.storage = spv::StorageClassPrivate;
            auto& original = get<SPIRVariable>(output.id);
            const uint32_t output_id = ids + 1;
            set<SPIRVariable>(output_id, output.type_id, spv::StorageClassOutput);
            ir.meta[output_id] = ir.meta[output.id];
            ir.meta.erase(output.id);
            set_name(output.id, "mg_output_" + std::to_string(output.id));
            original.basetype = ids;
            original.storage = spv::StorageClassPrivate;
            get_entry_point().interface_variables.push_back(output_id);

            const uint32_t count = type.array[0];
            get<SPIRFunction>(get_entry_point().self).fixup_hooks_out.push_back(
                [this, id = output.id, output_id, count]() {
                    for (uint32_t i = 0; i < count; ++i)
                        statement(to_name(output_id), "[", i, "] = ", to_name(id), "[", i, "];");
                });
        }
    }
};
