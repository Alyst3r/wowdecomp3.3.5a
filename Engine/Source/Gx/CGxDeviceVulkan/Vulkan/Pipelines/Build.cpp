// The pipeline of a key: the blend and topology tables, the vertex input of each program family,
// the pairing checks, the build, the pipeline table.
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Pipelines/Internal.hpp"

#include <cstring>

namespace
{
    using namespace gxvk::vk::pipelines::detail;

    /**
     * The colour blend state of one EGxBlend value (CGxDeviceD3d's s_srcBlend / s_dstBlend table):
     *
     *   0 Opaque        off                     7 InvSrcAlphaAdd    INVSRCALPHA / ONE
     *   1 AlphaKey      off (the test, not a blend)  8 InvSrcAlphaOpaque INVSRCALPHA / ZERO
     *   2 Alpha         SRCALPHA / INVSRCALPHA  9 SrcAlphaOpaque    SRCALPHA / ZERO
     *   3 Add           SRCALPHA / ONE         10 NoAlphaAdd        ONE / ONE
     *   4 Mod           DSTCOLOR / ZERO        11 ConstantAlpha     CONSTANT_COLOR / ONE_MINUS_CONSTANT_COLOR
     *   5 Mod2x         DSTCOLOR / SRCCOLOR
     *   6 ModAdd        DSTCOLOR / ONE
     *
     * The same factors for alpha as for colour, op ADD on both: D3D9 with SEPARATEALPHABLENDENABLE
     * off, which the engine never turns on. 11 reads the blend constants, left at Vulkan's default.
     */
    bool BlendState(uint32_t blend, VkPipelineColorBlendAttachmentState& a)
    {
        a.blendEnable  = VK_TRUE;
        a.colorBlendOp = VK_BLEND_OP_ADD;
        a.alphaBlendOp = VK_BLEND_OP_ADD;

        VkBlendFactor src = VK_BLEND_FACTOR_ONE;
        VkBlendFactor dst = VK_BLEND_FACTOR_ZERO;
        switch (blend)
        {
            case 0:
            case 1:  a.blendEnable = VK_FALSE; break;
            case 2:  src = VK_BLEND_FACTOR_SRC_ALPHA;           dst = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
            case 3:  src = VK_BLEND_FACTOR_SRC_ALPHA;           dst = VK_BLEND_FACTOR_ONE;                 break;
            case 4:  src = VK_BLEND_FACTOR_DST_COLOR;           dst = VK_BLEND_FACTOR_ZERO;                break;
            case 5:  src = VK_BLEND_FACTOR_DST_COLOR;           dst = VK_BLEND_FACTOR_SRC_COLOR;           break;
            case 6:  src = VK_BLEND_FACTOR_DST_COLOR;           dst = VK_BLEND_FACTOR_ONE;                 break;
            case 7:  src = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; dst = VK_BLEND_FACTOR_ONE;                 break;
            case 8:  src = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; dst = VK_BLEND_FACTOR_ZERO;                break;
            case 9:  src = VK_BLEND_FACTOR_SRC_ALPHA;           dst = VK_BLEND_FACTOR_ZERO;                break;
            case 10: src = VK_BLEND_FACTOR_ONE;                 dst = VK_BLEND_FACTOR_ONE;                 break;
            case 11: src = VK_BLEND_FACTOR_CONSTANT_COLOR;      dst = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR; break;
            default: return false;
        }

        a.srcColorBlendFactor = src;
        a.dstColorBlendFactor = dst;
        a.srcAlphaBlendFactor = src;
        a.dstAlphaBlendFactor = dst;
        a.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        return true;
    }

    /// An EGxPrim as a Vulkan topology (CGxDeviceD3d::s_primitiveConversion).
    bool Topology(uint32_t prim, VkPrimitiveTopology& out)
    {
        switch (prim)
        {
            case 0: out = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;     return true;
            case 1: out = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;      return true;
            case 2: out = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;     return true;
            case 3: out = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  return true;
            case 4: out = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; return true;
            case 5: out = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;   return true;
            default: return false;
        }
    }

    /**
     * The engine attribute (EGxVertexAttrib) that carries a translated-interface semantic
     * (manifest::Semantic): position0 -> 0, blendweight0 -> 1, blendindices0 -> 2, normal0 -> 3,
     * color0 -> 4, color1 -> 5, texcoordN -> 6 + N. False for fog0 / psize0, which no vertex carries.
     */
    bool AttribOfSemantic(uint32_t semantic, uint32_t& attrib)
    {
        switch (semantic)
        {
            case manifest::kPosition0:     attrib = 0; return true;
            case manifest::kBlendWeight0:  attrib = 1; return true;
            case manifest::kBlendIndices0: attrib = 2; return true;
            case manifest::kNormal0:       attrib = 3; return true;
            case manifest::kColor0:        attrib = 4; return true;
            case manifest::kColor1:        attrib = 5; return true;
            default:
                if (semantic >= manifest::kTexcoord0 && semantic < manifest::kTexcoord0 + 8)
                {
                    attrib = 6 + (semantic - manifest::kTexcoord0);
                    return true;
                }
                return false;
        }
    }

    /**
     * The attribute location a translated module declares for a semantic (the translator's
     * VIN_LOCATION): position0 0, normal0 1, color0 2, color1 3, texcoord0 4, texcoord1 5,
     * blendweight0 6, blendindices0 7; texcoord2..7 at 8..13 (no module names them).
     */
    bool LocationOfSemantic(uint32_t semantic, uint32_t& location)
    {
        switch (semantic)
        {
            case manifest::kPosition0:     location = 0; return true;
            case manifest::kNormal0:       location = 1; return true;
            case manifest::kColor0:        location = 2; return true;
            case manifest::kColor1:        location = 3; return true;
            case manifest::kTexcoord0:     location = 4; return true;
            case manifest::kTexcoord0 + 1: location = 5; return true;
            case manifest::kBlendWeight0:  location = 6; return true;
            case manifest::kBlendIndices0: location = 7; return true;
            default:
                if (semantic > manifest::kTexcoord0 + 1 && semantic < manifest::kBlendWeight0)
                {
                    location = 8 + (semantic - (manifest::kTexcoord0 + 2));
                    return true;
                }
                return false;
        }
    }

    struct VertexInput
    {
        VkVertexInputAttributeDescription attrs[kAttribCount + 2];
        uint32_t                          count     = 0;
        bool                              needConst = false;   ///< the constant stream is read
    };

    /// One attribute from the key's table, or from the constant stream when the vertex lacks it.
    void AddAttribute(const Key& k, VertexInput& in, uint32_t location, uint32_t attrib, VkFormat constFormat, uint32_t constOffset)
    {
        VkVertexInputAttributeDescription& a = in.attrs[in.count++];
        a.location = location;
        if (k.attribMask & (1u << attrib))
        {
            a.binding = k.attribs[attrib].binding;
            a.format  = k.attribs[attrib].format;
            a.offset  = k.attribs[attrib].offset;
        }
        else
        {
            a.binding    = k.streamCount;
            a.format     = constFormat;
            a.offset     = constOffset;
            in.needConst = true;
        }
    }

    /**
     * The vertex input of a key's vertex program.
     *
     *   FixedFunction, Ui        position at 0, colour at 1, texcoord 0 at 2 (the backend's interface)
     *   FixedFunctionSemantic    position0 at 0, color0 at 2, texcoord0 at 4, texcoord1 at 5
     *   Translated               the record's declared inputs, each at its semantic's location, each
     *                            required of the vertex
     *
     * A colour the vertex lacks reads opaque white, a texcoord zero, from the constant stream.
     */
    bool BuildVertexInput(const Key& k, VertexInput& in, const char*& why)
    {
        in.count     = 0;
        in.needConst = false;
        if (!(k.attribMask & (1u << kAttribPosition)))
        {
            why = "the vertex has no position";
            return false;
        }
        switch (static_cast<VertexProgram>(k.vertex))
        {
            case VertexProgram::FixedFunction:
            case VertexProgram::Ui:
                AddAttribute(k, in, 0, kAttribPosition, VK_FORMAT_R32G32B32_SFLOAT, 0);
                AddAttribute(k, in, 1, kAttribColor0, VK_FORMAT_B8G8R8A8_UNORM, kConstantColourOffset);
                AddAttribute(k, in, 2, kAttribTexCoord0, VK_FORMAT_R32G32_SFLOAT, kConstantTexcoordOffset);
                return true;

            case VertexProgram::FixedFunctionSemantic:
                AddAttribute(k, in, 0, kAttribPosition, VK_FORMAT_R32G32B32_SFLOAT, 0);
                AddAttribute(k, in, 2, kAttribColor0, VK_FORMAT_B8G8R8A8_UNORM, kConstantColourOffset);
                AddAttribute(k, in, 4, kAttribTexCoord0, VK_FORMAT_R32G32_SFLOAT, kConstantTexcoordOffset);
                AddAttribute(k, in, 5, kAttribTexCoord1, VK_FORMAT_R32G32_SFLOAT, kConstantTexcoordOffset);
                return true;

            case VertexProgram::Translated:
            {
                const manifest::Record* r = manifest::At(k.vertexRecord);
                if (!r)
                {
                    why = "the vertex record is gone";
                    return false;
                }
                for (uint32_t sem = 0; sem < manifest::kSemanticCount; ++sem)
                {
                    if (!(r->inputMask & (1u << sem))) continue;
                    uint32_t attrib = 0, location = 0;
                    if (!AttribOfSemantic(sem, attrib) || !LocationOfSemantic(sem, location))
                    {
                        why = "the vertex module reads a semantic no vertex carries";
                        return false;
                    }
                    if (!(k.attribMask & (1u << attrib)))
                    {
                        why = "the vertex module reads a semantic this vertex layout does not carry";
                        return false;
                    }
                    VkVertexInputAttributeDescription& a = in.attrs[in.count++];
                    a.location = location;
                    a.binding  = k.attribs[attrib].binding;
                    a.format   = k.attribs[attrib].format;
                    a.offset   = k.attribs[attrib].offset;
                }
                return true;
            }

            default:
                why = "an unknown vertex program";
                return false;
        }
    }

    /**
     * Whether the fragment program's varying reads are written by the vertex program: a Vulkan
     * fragment input must have a matching vertex output. The backend's own family is paired only
     * with itself; FixedFunctionSemantic writes every varying of the translated interface.
     */
    bool PairingOk(const Key& k, const char*& why)
    {
        const auto v = static_cast<VertexProgram>(k.vertex);
        const auto f = static_cast<FragmentProgram>(k.fragment);
        if (TranslatedFamily(v) != TranslatedFamily(f))
        {
            why = "the vertex and fragment programs are of different interfaces";
            return false;
        }
        if (!TranslatedFamily(v)) return true;

        uint32_t outputs = 0xFFFFFFFFu;
        if (v == VertexProgram::Translated)
        {
            const manifest::Record* r = manifest::At(k.vertexRecord);
            if (!r)
            {
                why = "the vertex record is gone";
                return false;
            }
            outputs = r->outputMask;
        }
        uint32_t reads = 0;
        if (f == FragmentProgram::Translated)
        {
            const manifest::Record* r = manifest::At(k.fragmentRecord);
            if (!r)
            {
                why = "the pixel record is gone";
                return false;
            }
            reads = r->readsMask;
        }
        else
            reads = (1u << manifest::kColor0) | (1u << manifest::kTexcoord0);
        if ((reads & ~outputs) != 0)
        {
            why = "the pixel module reads a varying the vertex module does not write";
            return false;
        }
        return true;
    }

    /// The name of a key's record for a log line, or the program's.
    const char* ProgramName(uint32_t program, uint16_t record, bool vertex)
    {
        if (const manifest::Record* r = manifest::At(record)) return manifest::FirstName(*r);
        static const char* kVertex[]   = { "FixedFunction", "Ui", "FixedFunctionSemantic", "Translated" };
        static const char* kFragment[] = { "FixedFunction", "Ui", "Desaturate", "FixedFunctionTranslated", "Translated" };
        if (vertex) return program < kVertexPrograms ? kVertex[program] : "?";
        return program < kFragmentPrograms ? kFragment[program] : "?";
    }

    void Refuse(const Key& k, const char* why)
    {
        ++s.stats.refusedKeys;
        Write(Warn, "vulkan: pipelines: a draw with vertex %s / pixel %s on attribute mask 0x%04X is refused: %s (draws with this key are skipped)",
              ProgramName(k.vertex, k.vertexRecord, true), ProgramName(k.fragment, k.fragmentRecord, false), k.attribMask, why);
    }

    /**
     * Builds the pipeline of a key.
     *
     * The vertex input has one binding per stream at the key's stride, plus the constant stream
     * at binding streamCount with stride 0 when a program reads an element the vertex lacks.
     * Viewport and scissor are dynamic; no culling, no depth (the pass has no depth attachment),
     * one sample.
     */
    bool Build(const Key& k, VkPipeline& out)
    {
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineColorBlendAttachmentState blend = {};
        if (!Topology(k.topology, topology) || !BlendState(k.blend, blend))
        {
            Refuse(k, "the topology or the blend mode is outside the tables");
            return false;
        }
        const char* why = "";
        if (!PairingOk(k, why))
        {
            Refuse(k, why);
            return false;
        }
        VertexInput in;
        if (!BuildVertexInput(k, in, why))
        {
            Refuse(k, why);
            return false;
        }
        const VkShaderModule vert = VertexModule(k);
        const VkShaderModule frag = FragmentModule(k);
        if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE)
        {
            Refuse(k, vert == VK_NULL_HANDLE ? "the vertex module could not be loaded" : "the pixel module could not be loaded");
            return false;
        }
        if (k.layoutIndex >= s.layoutCount)
        {
            Refuse(k, "the sampler-slot layout is gone");
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag;
        stages[1].pName  = "main";

        VkVertexInputBindingDescription bindings[kMaxStreams + 1] = {};
        uint32_t bindingCount = 0;
        for (uint32_t i = 0; i < k.streamCount; ++i)
        {
            bindings[bindingCount].binding   = i;
            bindings[bindingCount].stride    = k.stride[i];
            bindings[bindingCount].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            ++bindingCount;
        }
        if (in.needConst)
        {
            bindings[bindingCount].binding   = k.streamCount;
            bindings[bindingCount].stride    = 0;
            bindings[bindingCount].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            ++bindingCount;
        }

        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = bindingCount;
        vertexInput.pVertexBindingDescriptions      = bindings;
        vertexInput.vertexAttributeDescriptionCount = in.count;
        vertexInput.pVertexAttributeDescriptions    = in.attrs;

        VkPipelineInputAssemblyStateCreateInfo assembly = {};
        assembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = topology;

        VkPipelineViewportStateCreateInfo viewport = {};
        viewport.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo raster = {};
        raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_NONE;
        raster.frontFace   = VK_FRONT_FACE_CLOCKWISE;
        raster.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendStateCreateInfo blending = {};
        blending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blending.attachmentCount = 1;
        blending.pAttachments    = &blend;

        const VkDynamicState dynamics[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic = {};
        dynamic.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates    = dynamics;

        VkGraphicsPipelineCreateInfo info = {};
        info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.stageCount          = 2;
        info.pStages             = stages;
        info.pVertexInputState   = &vertexInput;
        info.pInputAssemblyState = &assembly;
        info.pViewportState      = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState   = &multisample;
        info.pColorBlendState    = &blending;
        info.pDynamicState       = &dynamic;
        info.layout              = s.layouts[k.layoutIndex].layout;
        info.renderPass          = k.pass;
        info.subpass             = 0;

        const VkResult r = s.dapi->CreateGraphicsPipelines(s.device, s.cache, 1, &info, nullptr, &out);
        if (r != VK_SUCCESS)
        {
            out = VK_NULL_HANDLE;
            Write(Error, "vulkan: pipelines: vkCreateGraphicsPipelines failed: %s (vertex %s, pixel %s, topology %u, blend %u, %u stream(s))",
                  ResultName(r), ProgramName(k.vertex, k.vertexRecord, true), ProgramName(k.fragment, k.fragmentRecord, false),
                  k.topology, k.blend, k.streamCount);
            ++s.stats.refusedKeys;
            return false;
        }
        ++s.stats.pipelineBuilds;
        return true;
    }
}

namespace gxvk::vk::pipelines::detail
{
    /// The entry of a key: found, or built and remembered -- a refused key too, with a null
    /// pipeline, so its draws are skipped without a second attempt. Null when the table is full.
    const Entry* Lookup(const Key& k)
    {
        for (uint32_t i = 0; i < s.entryCount; ++i)
            if (std::memcmp(&s.entries[i].key, &k, sizeof k) == 0) return &s.entries[i];
        if (s.entryCount == kMaxPipelines)
        {
            if (!s.warnedFull)
            {
                s.warnedFull = true;
                Write(Warn, "vulkan: pipelines: %u pipelines live, the table is full; further distinct draws are skipped (logged once)", kMaxPipelines);
            }
            return nullptr;
        }
        VkPipeline pipeline = VK_NULL_HANDLE;
        Build(k, pipeline);
        Entry& e = s.entries[s.entryCount++];
        // Bytewise, padding included: the lookup above compares the whole struct.
        std::memcpy(&e.key, &k, sizeof k);
        e.pipeline = pipeline;
        if (pipeline != VK_NULL_HANDLE) ++s.stats.pipelines;
        return &e;
    }

    /// Destroys every pipeline (the pass they were built against is gone). The caller has made the
    /// device idle.
    void DropPipelines()
    {
        for (uint32_t i = 0; i < s.entryCount; ++i)
            if (s.entries[i].pipeline != VK_NULL_HANDLE) s.dapi->DestroyPipeline(s.device, s.entries[i].pipeline, nullptr);
        s.entryCount      = 0;
        s.stats.pipelines = 0;
        s.bound           = Bound();
    }
}
