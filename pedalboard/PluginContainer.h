/*
 * pedalboard
 * Copyright 2021 Spotify AB
 *
 * Licensed under the GNU Public License, Version 3.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    https://www.gnu.org/licenses/gpl-3.0.html
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "JuceHeader.h"
#include <mutex>

#include "Plugin.h"

namespace Pedalboard {

/**
 * A class for all Pedalboard plugins that contain one or more other plugins.
 */
class PluginContainer : public Plugin {
public:
  PluginContainer(std::vector<std::shared_ptr<Plugin>> plugins)
      : plugins(plugins) {}
  virtual ~PluginContainer(){};

  std::vector<std::shared_ptr<Plugin>> &getPlugins() { return plugins; }

  /*
   * Get a flat list of all of the plugins contained
   * by this plugin, not including itself.
   */
  std::vector<std::shared_ptr<Plugin>> getAllPlugins() {
    std::vector<std::shared_ptr<Plugin>> flatList;
    for (auto plugin : plugins) {
      flatList.push_back(plugin);
      if (auto *pluginContainer =
              dynamic_cast<PluginContainer *>(plugin.get())) {
        auto children = pluginContainer->getAllPlugins();
        flatList.insert(flatList.end(), children.begin(), children.end());
      }
    }
    return flatList;
  }

protected:
  std::vector<std::shared_ptr<Plugin>> plugins;
};

inline void init_plugin_container(py::module &m) {
  py::class_<PluginContainer, Plugin, std::shared_ptr<PluginContainer>>(
      m, "PluginContainer",
      "A generic audio processing plugin that contains zero or more other "
      "plugins. Not intended for direct use.")
      .def(py::init([](std::vector<std::shared_ptr<Plugin>> plugins) {
        throw py::type_error(
            "PluginContainer is an abstract base class - don't instantiate "
            "this directly, use its subclasses instead.");
        // This will never be hit, but is required to provide a non-void
        // type to return from this lambda or else the compiler can't do
        // type inference.
        return nullptr;
      }))
      // Implement the Sequence protocol:
      .def("__getitem__",
           [](PluginContainer &s, size_t i) {
             if (i >= s.getPlugins().size())
               throw py::index_error("index out of range");
             return s.getPlugins()[i];
           })
      .def("__setitem__",
           [](PluginContainer &s, size_t i, std::shared_ptr<Plugin> plugin) {
             if (i >= s.getPlugins().size())
               throw py::index_error("index out of range");
             s.getPlugins()[i] = plugin;
           })
      .def("__delitem__",
           [](PluginContainer &s, size_t i) {
             if (i >= s.getPlugins().size())
               throw py::index_error("index out of range");
             auto &plugins = s.getPlugins();
             plugins.erase(plugins.begin() + i);
           })
      .def("__len__", [](PluginContainer &s) { return s.getPlugins().size(); })
      .def("insert",
           [](PluginContainer &s, int i, std::shared_ptr<Plugin> plugin) {
             if (i > s.getPlugins().size())
               throw py::index_error("index out of range");
             auto &plugins = s.getPlugins();
             plugins.insert(plugins.begin() + i, plugin);
           })
      .def("append",
           [](PluginContainer &s, std::shared_ptr<Plugin> plugin) {
             s.getPlugins().push_back(plugin);
           })
      .def("remove",
           [](PluginContainer &s, std::shared_ptr<Plugin> plugin) {
             auto &plugins = s.getPlugins();
             auto position = std::find(plugins.begin(), plugins.end(), plugin);
             if (position == plugins.end())
               throw py::value_error("remove(x): x not in list");
             plugins.erase(position);
           })
      .def(
          "__iter__",
          [](PluginContainer &s) {
            return py::make_iterator(s.getPlugins().begin(),
                                     s.getPlugins().end());
          },
          py::keep_alive<0, 1>())
      .def("__contains__",
           [](PluginContainer &s, std::shared_ptr<Plugin> plugin) {
             auto &plugins = s.getPlugins();
             return std::find(plugins.begin(), plugins.end(), plugin) !=
                    plugins.end();
           });
}

} // namespace Pedalboard