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
#ifndef NDRAI_DISCOVERY_H
#define NDRAI_DISCOVERY_H

#include <pxr/pxr.h>
#include "pxr/usd/ndrAi/api.h"

#include <pxr/usd/ndr/discoveryPlugin.h>

PXR_NAMESPACE_OPEN_SCOPE

class NdrAiDiscoveryPlugin : public NdrDiscoveryPlugin {
public:
    using Context = NdrDiscoveryPluginContext;

    NDRAI_API
    NdrAiDiscoveryPlugin();

    NDRAI_API
    ~NdrAiDiscoveryPlugin() override;

    NDRAI_API
    NdrNodeDiscoveryResultVec DiscoverNodes(const Context& context) override;

    const NdrStringVec& GetSearchURIs() const override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // NDRAI_DISCOVERY_H
