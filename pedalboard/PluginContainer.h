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
  PluginContainer(std::vector<std::shared_ptr<Plugin>> _plugins) {
    std::vector<std::shared_ptr<Plugin>> nonEffectPlugins;
    for (std::shared_ptr<Plugin> plugin : _plugins) {
      if (plugin && !plugin->acceptsAudioInput()) {
        nonEffectPlugins.push_back(plugin);
      }
    }

    if (!nonEffectPlugins.empty()) {
      std::string number = nonEffectPlugins.size() == 1 ? "One" : "Some";
      std::string description =
          nonEffectPlugins.size() == 1
              ? "is an instrument plugin, which does not accept"
              : "are instrument plugins, which do not accept";
      throw std::domain_error(number + " of the " +
                              std::to_string(nonEffectPlugins.size()) +
                              " provided plugins " + description +
                              " audio input. Instrument plugins cannot be "
                              "added to Pedalboard, Mix, "
                              "or Chain objects.");
    }

    plugins = _plugins;
  }

  virtual ~PluginContainer(){};

  std::vector<std::shared_ptr<Plugin>> &getPlugins() { return plugins; }

  /*
   * Get a flat list of all of the plugins contained
   * by this plugin, not including itself.
   */
  std::vector<std::shared_ptr<Plugin>> getAllPlugins() {
    std::vector<std::shared_ptr<Plugin>> flatList;
    for (auto plugin : plugins) {
      if (!plugin) {
        continue;
      }

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
      .def(
          py::init([](std::vector<std::shared_ptr<Plugin>> plugins) {
            throw py::type_error(
                "PluginContainer is an abstract base class - don't instantiate "
                "this directly, use its subclasses instead.");
            // This will never be hit, but is required to provide a non-void
            // type to return from this lambda or else the compiler can't do
            // type inference.
            return nullptr;
          }),
          py::arg("plugins"))
      // Implement the Sequence protocol:
      .def(
          "__getitem__",
          [](PluginContainer &s, int i) {
            std::scoped_lock lock(s.mutex);
            if (i < 0)
              i = s.getPlugins().size() + i;
            if (i < 0)
              throw py::index_error("index out of range");
            if (i >= s.getPlugins().size())
              throw py::index_error("index out of range");
            return s.getPlugins()[i];
          },
          py::arg("index"),
          "Get a plugin by its index. Index may be negative. If the index is "
          "out of range, an IndexError will be thrown.")
      .def(
          "__setitem__",
          [](PluginContainer &s, int i, std::shared_ptr<Plugin> plugin) {
            std::scoped_lock lock(s.mutex);
            if (i < 0)
              i = s.getPlugins().size() + i;
            if (i < 0)
              throw py::index_error("index out of range");
            if (i >= s.getPlugins().size())
              throw py::index_error("index out of range");

            if (plugin && !plugin->acceptsAudioInput()) {
              throw std::domain_error(
                  "Provided plugin is an instrument plugin "
                  "that does not accept audio input. Instrument plugins cannot "
                  "be added to Pedalboard, Mix, or Chain objects.");
            }

            s.getPlugins()[i] = plugin;
          },
          py::arg("index"), py::arg("plugin"),
          "Replace a plugin at the specified index. Index may be negative. If "
          "the index is out of range, an IndexError will be thrown.")
      .def(
          "__delitem__",
          [](PluginContainer &s, int i) {
            std::scoped_lock lock(s.mutex);
            if (i < 0)
              i = s.getPlugins().size() + i;
            if (i < 0)
              throw py::index_error("index out of range");
            if (i >= s.getPlugins().size())
              throw py::index_error("index out of range");
            auto &plugins = s.getPlugins();
            plugins.erase(plugins.begin() + i);
          },
          py::arg("index"),
          "Delete a plugin by its index. Index may be negative. If the index "
          "is out of range, an IndexError will be thrown.")
      .def(
          "__len__",
          [](PluginContainer &s) {
            std::scoped_lock lock(s.mutex);
            return s.getPlugins().size();
          },
          "Get the number of plugins in this container.")
      .def(
          "insert",
          [](PluginContainer &s, int i, std::shared_ptr<Plugin> plugin) {
            std::scoped_lock lock(s.mutex);
            if (i < 0)
              i = s.getPlugins().size() + i;
            if (i < 0)
              throw py::index_error("index out of range");
            if (i > s.getPlugins().size())
              throw py::index_error("index out of range");

            if (plugin && !plugin->acceptsAudioInput()) {
              throw std::domain_error(
                  "Provided plugin is an instrument plugin "
                  "that does not accept audio input. Instrument plugins cannot "
                  "be added to Pedalboard, Mix, or Chain objects.");
            }

            auto &plugins = s.getPlugins();
            plugins.insert(plugins.begin() + i, plugin);
          },
          py::arg("index"), py::arg("plugin"),
          "Insert a plugin at the specified index.")
      .def(
          "append",
          [](PluginContainer &s, std::shared_ptr<Plugin> plugin) {
            std::scoped_lock lock(s.mutex);

            if (plugin && !plugin->acceptsAudioInput()) {
              throw std::domain_error(
                  "Provided plugin is an instrument plugin "
                  "that does not accept audio input. Instrument plugins cannot "
                  "be added to Pedalboard, Mix, or Chain objects.");
            }

            s.getPlugins().push_back(plugin);
          },
          py::arg("plugin"), "Append a plugin to the end of this container.")
      .def(
          "remove",
          [](PluginContainer &s, std::shared_ptr<Plugin> plugin) {
            std::scoped_lock lock(s.mutex);
            auto &plugins = s.getPlugins();
            auto position = std::find(plugins.begin(), plugins.end(), plugin);
            if (position == plugins.end())
              throw py::value_error("remove(x): x not in list");
            plugins.erase(position);
          },
          py::arg("plugin"), "Remove a plugin by its value.")
      .def(
          "__iter__",
          [](PluginContainer &s) {
            return py::make_iterator(s.getPlugins().begin(),
                                     s.getPlugins().end());
          },
          py::keep_alive<0, 1>())
      .def(
          "__contains__",
          [](PluginContainer &s, std::shared_ptr<Plugin> plugin) {
            std::scoped_lock lock(s.mutex);
            auto &plugins = s.getPlugins();
            return std::find(plugins.begin(), plugins.end(), plugin) !=
                   plugins.end();
          },
          py::arg("plugin"));
}

} // namespace Pedalboard
