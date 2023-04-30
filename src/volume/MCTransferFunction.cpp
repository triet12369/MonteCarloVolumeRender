#include "pch.h"
#include <fstream>
#include "MCTransferFunction.h"
#include "nlohmann/json.hpp"

// initialize
MCTransferFunction::MCTransferFunction(std::string fileName) {
    nlohmann::json root;
    std::ifstream ifs(fileName);
    ifs >> root;

    opacityTF.Clear();
    diffuseTF.Clear();
    specularTF.Clear();
    emissionTF.Clear();
    roughnessTF.Clear();

    auto ExtractVec3FromJson = [](auto const& tree, auto const& key) -> Hawk::Math::Vec3 {
        Hawk::Math::Vec3 v{};
        uint32_t index = 0;
        for (auto& e : tree[key]) {
            v[index] = e.get<float>();
            index++;
        }
        return v;
    };

    for (auto const& e : root["NodesColor"]) {
        auto intensity = e["Intensity"].get<float>();
        auto diffuse = ExtractVec3FromJson(e, "Diffuse");
        auto specular = ExtractVec3FromJson(e, "Specular");
        auto roughness = e["Roughness"].get<float>();

        diffuseTF.AddNode(intensity, diffuse);
        specularTF.AddNode(intensity, specular);
        roughnessTF.AddNode(intensity, roughness);
    }

    for (auto const& e : root["NodesOpacity"])
        opacityTF.AddNode(e["Intensity"].get<F32>(), e["Opacity"].get<F32>());
}