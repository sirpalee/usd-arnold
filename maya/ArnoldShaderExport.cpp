// Copyright 2019 Luma Pictures
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "ArnoldShaderExport.h"

#include "pxr/base/tf/getenv.h"
#include "pxr/usd/usdAi/aiMaterialAPI.h"
#include "pxr/usd/usdShade/material.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MStringArray.h>

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

namespace {
AtNode* mtoa_export_node(const MObject& obj) {
    MStatus status;
    MFnDependencyNode node(obj, &status);
    if (!status) { return nullptr; }
    std::stringstream ss;
    ss << R"(arnoldScene -m "convert_selected" -list "nodes" )"
       << node.name().asChar();
    MStringArray result;
    MGlobal::executeCommand(ss.str().c_str(), result, false, false);
    if (result.length() == 0) { return nullptr; }
    return AiNodeLookUpByName(AtString(result[0].asChar()));
}
} // namespace

ArnoldShaderExport::ArnoldShaderExport(
    const UsdStageRefPtr& _stage, const UsdTimeCode& _time_code,
    bool strip_namespaces, const SdfPath& parent_scope,
    const UsdMayaUtil::MDagPathMap<SdfPath>& dag_to_usd)
    : AiShaderExport(_stage, parent_scope, _time_code),
      m_dag_to_usd(dag_to_usd),
      m_strip_namespaces(strip_namespaces) {
    const auto transform_assignment =
        TfGetenv("PXR_MAYA_TRANSFORM_ASSIGNMENT", "disable");
    if (transform_assignment == "common") {
        m_transform_assignment = TRANSFORM_ASSIGNMENT_COMMON;
    } else if (transform_assignment == "full") {
        m_transform_assignment = TRANSFORM_ASSIGNMENT_FULL;
    } else {
        m_transform_assignment = TRANSFORM_ASSIGNMENT_DISABLE;
    }
}

ArnoldShaderExport::~ArnoldShaderExport() {
    MGlobal::executeCommand("arnoldScene -m \"destroy\";");
}

SdfPath ArnoldShaderExport::export_shading_engine(MObject obj) {
    if (!obj.hasFn(MFn::kShadingEngine)) { return SdfPath(); }
    // we can't store the material in the map
    MFnDependencyNode node(obj);

    auto export_connected_node = [&node](const char* plug_name) -> AtNode* {
        auto plug = node.findPlug(plug_name);
        MPlugArray conns;
        plug.connectedTo(conns, true, false);
        if (conns.length() > 0) {
            auto obj = conns[0].node();
            return mtoa_export_node(obj);
        }
        return nullptr;
    };

    auto* surf_shader = export_connected_node("aiSurfaceShader");
    if (surf_shader == nullptr) {
        surf_shader = export_connected_node("surfaceShader");
    }
    auto* disp_shader = export_connected_node("displacementShader");

    // TODO: do proper name collision detection.
    // Note that this can happen regardless of strip_namespaces setting:
    //   strip_namespaces = false
    //     foo:bar > foo_bar
    //     foo_bar > foo_bar
    //   strip_namespaces = true
    //     foo:bar > bar
    //     bar     > bar
    // However, even the "main" plugin doesn't check for clashes when
    // strip_namespaces is false, so we're leaving this be for now...
    MString name = node.name();
    if (m_strip_namespaces) {
        // drop namespaces instead of making them part of the path
        // Could use UsdMayaUtil::stripNamespaces, but it's designed for working
        // with dag paths, and so is more complicated than we need, and also
        // adds a leading "|"
        int last_colon = name.rindexW(':');
        if (last_colon != -1) {
            name = name.substringW(last_colon + 1, name.length() - 1);
        }
    } else {
        name.substitute(":", "_");
    }
    return export_material(name.asChar(), surf_shader, disp_shader);
}

void ArnoldShaderExport::setup_shader(const MDagPath& dg, const SdfPath& path) {
    auto obj = dg.node();
    if (obj.hasFn(MFn::kTransform) || obj.hasFn(MFn::kLocator)) { return; }

    if (obj.hasFn(MFn::kPluginShape)) {
        MFnDependencyNode dn(obj);
        if (dn.typeName() == "vdb_visualizer") {
            auto* volume_node = mtoa_export_node(obj);
            static const AtString volumeString("volume");
            if (!AiNodeIs(volume_node, volumeString)) { return; }
            const auto* linked_shader = reinterpret_cast<const AtNode*>(
                AiNodeGetPtr(volume_node, "shader"));
            if (linked_shader == nullptr) { return; }
            std::string cleaned_volume_name = AiNodeGetName(linked_shader);
            clean_arnold_name(cleaned_volume_name);

            // start: below here is roughly the same as
            // AiShaderExport::export_material
            auto material_path =
                m_shaders_scope.AppendChild(TfToken(cleaned_volume_name));

            UsdAiMaterialAPI material;
            auto material_prim = m_stage->GetPrimAtPath(material_path);
            if (!material_prim.IsValid()) {
                material = UsdAiMaterialAPI(
                    UsdShadeMaterial::Define(m_stage, material_path));
            } else {
                // FIXME: why would a material already exist for a
                // vdb_visualizer?
                material = UsdAiMaterialAPI(material_prim);
            }

            auto linked_path = export_arnold_node(linked_shader, material_path);
            if (!linked_path.IsEmpty()) {
                auto rel = material.GetSurfaceRel();
                if (rel) {
                    rel.ClearTargets(true);
                    rel.AddTarget(linked_path);
                } else {
                    rel = material.CreateSurfaceRel();
                    rel.AddTarget(linked_path);
                }
            }
            // end
            bind_material(material_path, path);
            return;
        }
    }

    auto get_shading_engine_obj = [](MObject shape_obj,
                                     unsigned int instance_num) -> MObject {
        constexpr auto out_shader_plug = "instObjGroups";

        MFnDependencyNode node(shape_obj);
        auto plug = node.findPlug(out_shader_plug);
        MPlugArray conns;
        plug = plug.elementByLogicalIndex(instance_num);
            plug.connectedTo(conns, false, true);
        auto conns_length = conns.length();
        for (auto i = decltype(conns_length){0}; i < conns_length; ++i) {
            const auto splug = conns[i];
            const auto sobj = splug.node();
            if (sobj.apiType() == MFn::kShadingEngine) { return sobj; }
        }
        // Object is empty, take a look at per face assignments and grab the first one.
        MObject obg = MFnDependencyNode(shape_obj).attribute("objectGroups");
        plug = plug.child(obg);
        int elements = plug.evaluateNumElements();
        for (int j = 0; j < elements; ++j) {
            plug.elementByPhysicalIndex(j).connectedTo(conns, false, true);
            conns_length = conns.length();
            for (auto i = decltype(conns_length){0}; i < conns_length; ++i) {
                const auto splug = conns[i];
                const auto sobj = splug.node();
                if (sobj.apiType() == MFn::kShadingEngine) { return sobj; }
            }
        }
        return MObject();
    };

    auto is_initial_group = [](MObject tobj) -> bool {
        constexpr auto initial_shading_group_name = "initialShadingGroup";
        return MFnDependencyNode(tobj).name() == initial_shading_group_name;
    };

    auto material_assignment = get_shading_engine_obj(obj, dg.instanceNumber());
    // we are checking for transform assignments as well
    if (m_transform_assignment == TRANSFORM_ASSIGNMENT_FULL &&
        (material_assignment.isNull() ||
         is_initial_group(material_assignment))) {
        auto dag_it = dg;
        dag_it.pop();
        for (; dag_it.length() > 0; dag_it.pop()) {
            const auto it = m_dag_to_usd.find(dag_it);
            if (it != m_dag_to_usd.end()) {
                const auto transform_assignment = get_shading_engine_obj(
                    dag_it.node(), dag_it.instanceNumber());
                if (!transform_assignment.isNull() &&
                    !is_initial_group(transform_assignment)) {
                    const auto shader_path =
                        export_shading_engine(transform_assignment);
                    if (shader_path.IsEmpty()) { return; }
                    bind_material(shader_path, it->second.GetPrimPath());
                    return;
                }
            }
        }
    }

    if (material_assignment.isNull()) {
        material_assignment = get_shading_engine_obj(obj, 0);
    }

    const auto shader_path = export_shading_engine(material_assignment);
    if (shader_path.IsEmpty()) { return; }
    bind_material(shader_path, path);
}

void ArnoldShaderExport::setup_shaders() {
    for (const auto& it : m_dag_to_usd) { setup_shader(it.first, it.second); }
    if (m_transform_assignment == TRANSFORM_ASSIGNMENT_COMMON) {
        collapse_shaders();
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
