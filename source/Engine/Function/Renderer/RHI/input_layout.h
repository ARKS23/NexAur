#pragma once

#include <cstring>

#include "graphics_types.h"


namespace NexAur {
    static bool SafeStrEqual(const char* Str0, const char* Str1) {
        if ((Str0 == NULL) != (Str1 == NULL))
            return false;

        if (Str0 != NULL && Str1 != NULL)
            return strcmp(Str0, Str1) == 0;

        return true;
    }

    // 自动计算偏移宏
    static constexpr uint32_t LAYOUT_ELEMENT_AUTO_OFFSET = 0xFFFFFFFF;
    static constexpr uint32_t LAYOUT_ELEMENT_AUTO_STRIDE = 0xFFFFFFFF;

    // 布局最大元素个数
    static constexpr uint32_t MAX_LAYOUT_ELEMENTS = 16;

    enum class InputElementFrequency : uint8_t {
        Undefined = 0,
        PerVertex,     // 每个顶点读一次数据
        PerInstance,    // 每个实例读一次数据
        NumFrequencys
    };


    // 单个元素布局信息
    struct LayoutElement {
        // 风格标记， GLSL中不用
        const char* HLSL_semantic = "ATTRIB";

        // 输入索引
        uint32_t input_index = 0;

        // 缓冲槽位
        uint32_t buffer_slot = 0;

        // 向量维度
        uint32_t num_components = 0;

        // 数据类型
        ValueType value_type{ValueType::Float32};

        // 标准化
        bool is_normalized = true;

        // 相对偏移
        uint32_t relative_offset = LAYOUT_ELEMENT_AUTO_OFFSET;

        // 步长，两个顶点之间的距离字节数
        uint32_t stride = LAYOUT_ELEMENT_AUTO_STRIDE;

        // 指针移动频率
        InputElementFrequency frequency{InputElementFrequency::PerVertex};

        // 步进率，配合per_instance使用
        uint32_t instance_data_step_rate = 1;


        constexpr LayoutElement() noexcept{}

        constexpr LayoutElement(uint32_t                   _InputIndex,
                                uint32_t                   _BufferSlot,
                                uint32_t                   _NumComponents,
                                ValueType               _ValueType,
                                bool                     _IsNormalized         = LayoutElement{}.is_normalized,
                                uint32_t                   _RelativeOffset       = LayoutElement{}.relative_offset,
                                uint32_t                   _Stride               = LayoutElement{}.stride,
                                InputElementFrequency  _Frequency            = LayoutElement{}.frequency,
                                uint32_t                   _InstanceDataStepRate = LayoutElement{}.instance_data_step_rate)noexcept :
            input_index          {_InputIndex          },
            buffer_slot          {_BufferSlot          },
            num_components       {_NumComponents       },
            value_type           {_ValueType           },
            is_normalized        {_IsNormalized        },
            relative_offset      {_RelativeOffset      },
            stride              {_Stride              },
            frequency           {_Frequency           },
            instance_data_step_rate{_InstanceDataStepRate}
        {}

        constexpr LayoutElement(uint32_t                   _InputIndex,
                                uint32_t                   _BufferSlot,
                                uint32_t                   _NumComponents,
                                ValueType               _ValueType,
                                bool                     _IsNormalized,
                                InputElementFrequency  _Frequency,
                                uint32_t                   _InstanceDataStepRate = LayoutElement{}.instance_data_step_rate)noexcept :
            input_index          {_InputIndex                   },
            buffer_slot          {_BufferSlot                   },
            num_components       {_NumComponents                },
            value_type           {_ValueType                    },
            is_normalized        {_IsNormalized                 },
            relative_offset      {LayoutElement{}.relative_offset},
            stride              {LayoutElement{}.stride        },
            frequency           {_Frequency                    },
            instance_data_step_rate{_InstanceDataStepRate         }
        {}

        bool operator == (const LayoutElement& rhs) const {
            return input_index           == rhs.input_index            &&
                buffer_slot           == rhs.buffer_slot            &&
                num_components        == rhs.num_components         &&
                value_type            == rhs.value_type             &&
                is_normalized         == rhs.is_normalized          &&
                relative_offset       == rhs.relative_offset        &&
                stride               == rhs.stride                &&
                frequency            == rhs.frequency             &&
                instance_data_step_rate == rhs.instance_data_step_rate  &&
                SafeStrEqual(HLSL_semantic, rhs.HLSL_semantic);
        }

        bool operator != (const LayoutElement& rhs) const {
            return !(*this == rhs);
        }
    };

    
    // 输入数据布局信息
    struct InputLayoutDesc {
        // 元素布局数组
        const LayoutElement* layout_elements = nullptr;

        // 元素数量
        uint32_t num_elements = 0;

        bool operator == (const InputLayoutDesc& rhs) const {
            if (num_elements != rhs.num_elements)
                return false;

            for (uint32_t i = 0; i < num_elements; ++i) {
                if (layout_elements[i] != rhs.layout_elements[i])
                    return false;
            }

            return true;
        }

        bool operator != (const InputLayoutDesc& rhs) const {
            return !(*this == rhs);
        }
    };
} // namespace NexAur

