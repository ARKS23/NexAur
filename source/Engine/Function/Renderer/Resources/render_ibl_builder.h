#pragma once
#include "Core/Base.h"

namespace NexAur {
    class TextureCubeMap;
    class Texture2D;

    struct RenderEnvironmentMap {
        std::shared_ptr<TextureCubeMap> skybox_texture;
        std::shared_ptr<TextureCubeMap> irradiance_map;
        std::shared_ptr<TextureCubeMap> prefilter_map;
        std::shared_ptr<Texture2D> brdf_lut_map;
    };

    class NEXAUR_API RenderIBLBuilder {
    public:
        static std::shared_ptr<RenderEnvironmentMap> bakeFromHDR(const std::string& hdr_path);
        static void shutdown();

    private:
        static std::shared_ptr<TextureCubeMap> generateSkybox(std::shared_ptr<Texture2D> hdr_texture);
        static std::shared_ptr<TextureCubeMap> generateIrradianceMap(std::shared_ptr<TextureCubeMap> skybox_texture);
        static std::shared_ptr<TextureCubeMap> generatePrefilterMap(std::shared_ptr<TextureCubeMap> skybox_texture);
        static std::shared_ptr<Texture2D> generateBRDFLUT();

    private:
        static std::shared_ptr<Texture2D> s_brdf_lut_map;
    };
} // namespace NexAur
