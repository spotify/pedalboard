Pedalboard Internals
====================

Pedalboard is a Python wrapper around JUCE, an open-source cross-platform C++ library for developing audio applications.
Most of Pedalboard is written in C++ using pybind11, a library for binding C++ code to Python. The design goals of
Pedalboard are to:

 - Expose the digital signal processing functionality of JUCE to Python code in an idiomatic way
 - Provide the raw speed and performance of JUCE's DSP code to Python
 - Hide the complexity of JUCE's C++ interface and show programmers an intuitive API
 

Adding new built-in Pedalboard plugins
--------------------------------------

Pedalboard supports loading VST3® and Audio Unit plugins, but it can also be useful to add new plugins to Pedalboard itself, for a variety of reasons:

 - **Wider compatibility**: Not all plugins are supported on every platform. (Notably, VST3® plugins that support Linux are rare.)
 - **Fewer dependencies**: ``import pedalboard`` is simpler than ``pedalboard.load_plugin(...)``, and removes the need to copy around plugin files and any associated resources.
 - **More options**: Available plugins might not include all of the required options; for instance, plugins meant for musical use may not include the appropriate options to be used for machine learning use cases, or vice versa.
 - **Stability**: Some VST3® or Audio Unit plugins might have compatibility issues with Pedalboard or in environments where Pedalboard is used. Implementing the same effect within Pedalboard itself ensures that the logic is portable, well tested, thread-safe, and performant.


Design considerations
^^^^^^^^^^^^^^^^^^^^^

When adding a new Pedalboard effect plugin, the following constraints should be considered:

 - **Licensing**: is the code that implements the new effect compatible with the GPLv3 license used for Pedalboard? Can it be statically linked?
 - **Vendorability**: will the code have to be added to Pedalboard's repository, or can it be included as a Git submodule?
 - **Binary Size**: Pedalboard is distributed as a binary wheel. Will the new code required (or any required resources) increase the binary size by a considerable amount?
 - **Platform Compatibility**: Does the new effect's code compile on macOS, Windows, and Linux?
 - **Dependencies**: Does the new code require any runtime dependencies that cannot be included in the Pedalboard binary? Requiring users to install additional files or have additional software installed, for example, would complicate the "just `import pedalboard`" simplicity of the package.
